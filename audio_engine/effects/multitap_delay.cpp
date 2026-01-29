#include "multitap_delay.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

MultiTapDelayState* multitap_delay_state_create(int num_taps,
                                                  const float* tap_times_ms,
                                                  const float* tap_gains,
                                                  float feedback,
                                                  float mix,
                                                  float sample_rate) {
    MultiTapDelayState* state = static_cast<MultiTapDelayState*>(
        aligned_alloc(64, sizeof(MultiTapDelayState)));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;

    // Calculate maximum delay needed (2 seconds max)
    state->max_delay_samples = static_cast<uint32_t>(sample_rate * 2.0f);
    state->buffer_size = state->max_delay_samples;

    // Allocate buffer
    state->buffer = static_cast<float*>(
        aligned_alloc(64, state->buffer_size * sizeof(float)));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }

    memset(state->buffer, 0, state->buffer_size * sizeof(float));
    state->write_pos = 0;

    // Initialize taps
    state->num_active_taps = std::min(num_taps, MultiTapDelayState::MAX_TAPS);

    for (int t = 0; t < MultiTapDelayState::MAX_TAPS; t++) {
        if (t < state->num_active_taps) {
            // Initialize active tap
            state->taps[t].enabled = true;

            state->taps[t].time_ms.base = tap_times_ms[t];
            state->taps[t].time_ms.mod = 0.0f;
            state->taps[t].time_ms.smoothed = tap_times_ms[t];
            state->taps[t].time_ms.set_smoothing(5.0f, sample_rate);  // 5ms smoothing

            state->taps[t].gain.base = tap_gains[t];
            state->taps[t].gain.mod = 0.0f;
            state->taps[t].gain.smoothed = tap_gains[t];
            state->taps[t].gain.smooth_coeff = 0.0f;  // No smoothing needed
        } else {
            // Initialize inactive tap
            state->taps[t].enabled = false;

            state->taps[t].time_ms.base = 0.0f;
            state->taps[t].time_ms.mod = 0.0f;
            state->taps[t].time_ms.smoothed = 0.0f;
            state->taps[t].time_ms.smooth_coeff = 0.0f;

            state->taps[t].gain.base = 0.0f;
            state->taps[t].gain.mod = 0.0f;
            state->taps[t].gain.smoothed = 0.0f;
            state->taps[t].gain.smooth_coeff = 0.0f;
        }
    }

    // Initialize global feedback parameter (no smoothing)
    state->feedback.base = feedback;
    state->feedback.mod = 0.0f;
    state->feedback.smoothed = feedback;
    state->feedback.smooth_coeff = 0.0f;

    // Initialize global mix parameter (no smoothing)
    state->mix.base = mix;
    state->mix.mod = 0.0f;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

void multitap_delay_state_destroy(MultiTapDelayState* state) {
    if (!state) return;
    if (state->buffer) free(state->buffer);
    free(state);
}

void multitap_delay_op(DSPBlock& b, float* buffers, int n) {
    MultiTapDelayState* s = static_cast<MultiTapDelayState*>(b.state);
    if (!s) {
        // Passthrough fallback if no state
        float* in = &buffers[b.in_a * n];
        float* out = &buffers[b.out * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];
    float* ring = s->buffer;
    const uint32_t ring_size = s->buffer_size;

    const float feedback = std::clamp(s->feedback.get(), 0.0f, 0.99f);
    const float mix = std::clamp(s->mix.get(), 0.0f, 1.0f);
    const float dry = 1.0f - mix;

    uint32_t write_pos = s->write_pos;

    for (int i = 0; i < n; i++) {
        float input_sample = in[i];
        float tap_sum = 0.0f;

        // Read from all active taps
        for (int t = 0; t < s->num_active_taps; t++) {
            if (!s->taps[t].enabled) continue;

            // Get smoothed tap parameters
            float tap_time_ms = s->taps[t].time_ms.get_smoothed();
            float tap_gain = s->taps[t].gain.get();

            // Convert time to samples and clamp
            uint32_t tap_delay = static_cast<uint32_t>(
                tap_time_ms * s->sample_rate / 1000.0f);
            tap_delay = std::clamp(tap_delay, 1u, s->max_delay_samples - 1);

            // Read from delay line
            uint32_t read_pos = (write_pos + ring_size - tap_delay) % ring_size;
            float tap_sample = ring[read_pos];

            // Apply tap gain and accumulate
            tap_sum += tap_sample * tap_gain;
        }

        // Write input + feedback to delay line
        ring[write_pos] = input_sample + tap_sum * feedback;

        // Output: dry/wet mix
        out[i] = input_sample * dry + tap_sum * mix;

        // Advance write position
        write_pos = (write_pos + 1) % ring_size;
    }

    s->write_pos = write_pos;
}
