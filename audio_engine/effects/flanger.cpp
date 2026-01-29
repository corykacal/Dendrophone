#include "flanger.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

FlangerState* flanger_state_create(float rate_hz,
                                     float depth,
                                     float feedback,
                                     float delay_ms,
                                     float mix,
                                     float sample_rate) {
    FlangerState* state = static_cast<FlangerState*>(
        aligned_alloc(64, sizeof(FlangerState)));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;

    // Allocate delay buffer (20ms max)
    state->buffer_size = static_cast<uint32_t>(sample_rate * 0.02f);
    state->buffer = static_cast<float*>(
        aligned_alloc(64, state->buffer_size * sizeof(float)));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }
    memset(state->buffer, 0, state->buffer_size * sizeof(float));

    state->write_pos = 0;
    state->lfo_phase = 0.0f;

    // Initialize parameters
    state->rate_hz.base = rate_hz;
    state->rate_hz.smoothed = rate_hz;
    state->rate_hz.set_smoothing(20.0f, sample_rate);

    state->depth.base = depth;
    state->depth.smoothed = depth;
    state->depth.smooth_coeff = 0.0f;

    state->feedback.base = feedback;
    state->feedback.smoothed = feedback;
    state->feedback.set_smoothing(10.0f, sample_rate);

    state->delay_ms.base = delay_ms;
    state->delay_ms.smoothed = delay_ms;
    state->delay_ms.set_smoothing(30.0f, sample_rate);

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

void flanger_state_destroy(FlangerState* state) {
    if (!state) return;
    if (state->buffer) free(state->buffer);
    free(state);
}

void flanger_op(DSPBlock& b, float* buffers, int n) {
    FlangerState* s = static_cast<FlangerState*>(b.state);
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
    float feedback = std::clamp(s->feedback.get_smoothed(), -0.95f, 0.95f);
    float center_delay = std::clamp(s->delay_ms.get_smoothed(), 0.1f, 10.0f);

    // Flanger modulation range (typically ±50% of center delay)
    const float center_delay_samples = center_delay * s->sample_rate / 1000.0f;
    const float mod_range_samples = center_delay_samples * 0.5f;

    const float lfo_increment = 2.0f * M_PI * rate / s->sample_rate;

    for (int i = 0; i < n; i++) {
        // Generate LFO (sine wave, -1 to 1)
        float lfo = sinf(s->lfo_phase);

        // Calculate modulated delay time
        float delay_samples = center_delay_samples + lfo * mod_range_samples * depth;
        delay_samples = std::clamp(delay_samples, 1.0f, static_cast<float>(s->buffer_size - 2));

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

        // Apply feedback and write to buffer
        s->buffer[s->write_pos] = in[i] + delayed_sample * feedback;

        // Mix dry and wet signals
        out[i] = in[i] * dry + delayed_sample * mix;

        // Advance positions
        s->write_pos = (s->write_pos + 1) % s->buffer_size;
        s->lfo_phase += lfo_increment;
        if (s->lfo_phase >= 2.0f * M_PI) {
            s->lfo_phase -= 2.0f * M_PI;
        }
    }
}
