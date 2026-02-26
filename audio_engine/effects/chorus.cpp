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
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    ChorusState* state = static_cast<ChorusState*>(
        aligned_alloc(64, align_up(sizeof(ChorusState))));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;

    // Allocate delay buffer (100ms max)
    state->buffer_size = static_cast<uint32_t>(sample_rate * 0.1f);
    state->buffer = static_cast<float*>(
        aligned_alloc(64, align_up(state->buffer_size * sizeof(float))));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }
    memset(state->buffer, 0, state->buffer_size * sizeof(float));

    state->write_pos = 0;
    state->num_voices = std::clamp(voices, 2, ChorusState::MAX_VOICES);

    // Initialize magic-circle LFO state (voices spread evenly in phase)
    float initial_phase_step = (2.0f * M_PI) / state->num_voices;
    for (int i = 0; i < ChorusState::MAX_VOICES; i++) {
        float phase = initial_phase_step * i;
        state->lfo_cos[i] = cosf(phase);
        state->lfo_sin[i] = sinf(phase);
    }
    float omega = M_PI * rate_hz / sample_rate;
    state->lfo_epsilon = 2.0f * sinf(omega);
    state->last_rate   = rate_hz;

    // Initialize parameters
    state->rate_hz.base = rate_hz;
    state->rate_hz.smoothed = rate_hz;
    state->rate_hz.set_smoothing(20.0f, sample_rate);

    state->depth.base = depth;
    state->depth.smoothed = depth;
    state->depth.set_smoothing(20.0f, sample_rate);

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

    // Recompute magic-circle epsilon if rate has changed this block
    float rate = std::clamp(s->rate_hz.get_smoothed(), 0.1f, 10.0f);
    if (rate != s->last_rate) {
        s->lfo_epsilon = 2.0f * sinf(M_PI * rate / s->sample_rate);
        s->last_rate = rate;
    }
    float depth = std::clamp(s->depth.get_smoothed(), 0.0f, 1.0f);

    // Chorus parameters
    const float base_delay_ms = 20.0f;
    const float mod_range_ms  = 5.0f;

    const float base_delay_samples = base_delay_ms * s->sample_rate / 1000.0f;
    const float mod_range_samples  = mod_range_ms  * s->sample_rate / 1000.0f;
    const float stereo_spread      = s->stereo_width * 4.0f * s->sample_rate / 1000.0f;

    for (int i = 0; i < n; i++) {
        // Write to buffer
        s->buffer[s->write_pos] = in[i];

        float chorus_sum = 0.0f;
        for (int v = 0; v < s->num_voices; v++) {
            // 1. Magic-circle LFO step (replaces sinf per sample)
            float new_sin = s->lfo_sin[v] + s->lfo_epsilon * s->lfo_cos[v];
            float new_cos = s->lfo_cos[v] - s->lfo_epsilon * new_sin;
            s->lfo_sin[v] = new_sin;
            s->lfo_cos[v] = new_cos;

            // 2. Per-voice base delay: even voices pushed +, odd voices pushed -
            float voice_sign = (v % 2 == 0) ? 1.0f : -1.0f;
            float voice_base_delay = base_delay_samples + voice_sign * stereo_spread;

            // 3. Modulated delay time
            float delay_samples = std::clamp(
                voice_base_delay + new_sin * mod_range_samples * depth,
                1.0f, static_cast<float>(s->buffer_size - 2));

            // 4. Hermite cubic fractional delay read
            float read_pos_f = (float)s->write_pos - delay_samples;
            if (read_pos_f < 0.0f) read_pos_f += s->buffer_size;
            uint32_t ri = (uint32_t)read_pos_f % s->buffer_size;
            float frac = read_pos_f - floorf(read_pos_f);

            float p0 = s->buffer[(ri + s->buffer_size - 1) % s->buffer_size];
            float p1 = s->buffer[ri];
            float p2 = s->buffer[(ri + 1) % s->buffer_size];
            float p3 = s->buffer[(ri + 2) % s->buffer_size];

            float a = -0.5f*p0 + 1.5f*p1 - 1.5f*p2 + 0.5f*p3;
            float b =  p0 - 2.5f*p1 + 2.0f*p2 - 0.5f*p3;
            float c = -0.5f*p0 + 0.5f*p2;
            chorus_sum += ((a * frac + b) * frac + c) * frac + p1;
        }

        // Average chorus voices and mix with dry signal
        float wet = chorus_sum / s->num_voices;
        out[i] = in[i] * dry + wet * mix;

        // Advance write position
        s->write_pos = (s->write_pos + 1) % s->buffer_size;
    }
}
