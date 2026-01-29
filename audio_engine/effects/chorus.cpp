#include "chorus.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ChorusState* chorus_state_create(float rate_hz,
                                   float depth,
                                   int voices,
                                   float stereo_width,
                                   float mix,
                                   float sample_rate) {
    ChorusState* state = static_cast<ChorusState*>(
        aligned_alloc(64, sizeof(ChorusState)));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;

    // Allocate delay buffer (100ms max)
    state->buffer_size = static_cast<uint32_t>(sample_rate * 0.1f);
    state->buffer = static_cast<float*>(
        aligned_alloc(64, state->buffer_size * sizeof(float)));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }
    memset(state->buffer, 0, state->buffer_size * sizeof(float));

    state->write_pos = 0;
    state->num_voices = std::clamp(voices, 2, ChorusState::MAX_VOICES);

    // Initialize LFO phases (spread evenly)
    for (int i = 0; i < ChorusState::MAX_VOICES; i++) {
        state->lfo_phase[i] = (2.0f * M_PI * i) / state->num_voices;
    }

    // Initialize parameters
    state->rate_hz.base = rate_hz;
    state->rate_hz.smoothed = rate_hz;
    state->rate_hz.set_smoothing(20.0f, sample_rate);

    state->depth.base = depth;
    state->depth.smoothed = depth;
    state->depth.smooth_coeff = 0.0f;

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    state->stereo_width = stereo_width;

    return state;
}

void chorus_state_destroy(ChorusState* state) {
    if (!state) return;
    if (state->buffer) free(state->buffer);
    free(state);
}

void chorus_op(DSPBlock& b, float* buffers, int n) {
    ChorusState* s = static_cast<ChorusState*>(b.state);
    if (!s) {
        float* in = &buffers[b.in_a * n];
        float* out = &buffers[b.out * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    const float mix = std::clamp(s->mix.get(), 0.0f, 1.0f);
    const float dry = 1.0f - mix;

    // Get parameters
    float rate = std::clamp(s->rate_hz.get_smoothed(), 0.1f, 10.0f);
    float depth = std::clamp(s->depth.get(), 0.0f, 1.0f);

    // Chorus parameters
    const float base_delay_ms = 20.0f;  // Center delay time
    const float mod_range_ms = 5.0f;    // Modulation range (±5ms)

    const float base_delay_samples = base_delay_ms * s->sample_rate / 1000.0f;
    const float mod_range_samples = mod_range_ms * s->sample_rate / 1000.0f;

    const float lfo_increment = 2.0f * M_PI * rate / s->sample_rate;

    for (int i = 0; i < n; i++) {
        // Write to buffer
        s->buffer[s->write_pos] = in[i];

        // Sum chorus voices
        float chorus_sum = 0.0f;
        for (int v = 0; v < s->num_voices; v++) {
            // Generate LFO (sine wave)
            float lfo = sinf(s->lfo_phase[v]);

            // Calculate modulated delay time
            float delay_samples = base_delay_samples + lfo * mod_range_samples * depth;
            delay_samples = std::clamp(delay_samples, 10.0f, static_cast<float>(s->buffer_size - 10));

            // Calculate read position with interpolation
            float read_pos_float = s->write_pos - delay_samples;
            if (read_pos_float < 0.0f) {
                read_pos_float += s->buffer_size;
            }

            uint32_t read_pos = static_cast<uint32_t>(read_pos_float);
            uint32_t read_pos_next = (read_pos + 1) % s->buffer_size;
            float frac = read_pos_float - floorf(read_pos_float);

            // Linear interpolation
            float sample_a = s->buffer[read_pos];
            float sample_b = s->buffer[read_pos_next];
            float delayed_sample = sample_a * (1.0f - frac) + sample_b * frac;

            chorus_sum += delayed_sample;

            // Advance LFO phase
            s->lfo_phase[v] += lfo_increment;
            if (s->lfo_phase[v] >= 2.0f * M_PI) {
                s->lfo_phase[v] -= 2.0f * M_PI;
            }
        }

        // Average chorus voices and mix with dry signal
        float wet = chorus_sum / s->num_voices;
        out[i] = in[i] * dry + wet * mix;

        // Advance write position
        s->write_pos = (s->write_pos + 1) % s->buffer_size;
    }
}
