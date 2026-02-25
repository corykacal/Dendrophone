#include "delay.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

// Create delay state - call from control thread only
DelayState* delay_state_create(uint32_t max_delay_samples,
                                uint32_t delay_samples,
                                float feedback,
                                float mix,
                                float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };

    DelayState* state = static_cast<DelayState*>(
        aligned_alloc(64, align_up(sizeof(DelayState))));
    if (!state) return nullptr;

    state->buffer = static_cast<float*>(
        aligned_alloc(64, align_up(max_delay_samples * sizeof(float))));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }

    memset(state->buffer, 0, max_delay_samples * sizeof(float));
    state->buffer_size = max_delay_samples;
    state->write_pos = 0;
    state->max_delay_samples = max_delay_samples;
    state->sample_rate = sample_rate;

    // Initialize modulatable parameters
    float time_ms = delay_samples * 1000.0f / sample_rate;
    state->time_ms.base = time_ms;
    state->time_ms.mod = 0.0f;
    state->time_ms.smoothed = time_ms;
    state->time_ms.set_smoothing(5.0f, sample_rate);  // 5ms smoothing to avoid clicks

    state->feedback.base = feedback;
    state->feedback.mod = 0.0f;
    state->feedback.smoothed = feedback;
    state->feedback.smooth_coeff = 0.0f;  // No smoothing needed

    state->mix.base = mix;
    state->mix.mod = 0.0f;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

// Destroy delay state - call from control thread only
void delay_state_destroy(DelayState* state) {
    if (!state) return;
    if (state->buffer) free(state->buffer);
    free(state);
}

// RT-safe delay processing
void delay_op(DSPBlock& b, float* buffers, int n) {
    DelayState* state = static_cast<DelayState*>(b.state);
    if (!state) {
        // Fallback: passthrough if no state
        float* in = &buffers[b.in_a * n];
        float* out = &buffers[b.out * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];
    float* ring = state->buffer;
    const uint32_t ring_size = state->buffer_size;

    // Get modulated parameter values (smoothed for time to avoid clicks)
    float time_ms = state->time_ms.get_smoothed();
    uint32_t delay = static_cast<uint32_t>(time_ms * state->sample_rate / 1000.0f);
    delay = std::min(delay, state->max_delay_samples - 1);
    delay = std::max(delay, 1u);

    const float feedback = std::clamp(state->feedback.get(), 0.0f, 0.99f);
    const float mix = std::clamp(state->mix.get(), 0.0f, 1.0f);
    const float dry = 1.0f - mix;

    uint32_t write_pos = state->write_pos;

    for (int i = 0; i < n; i++) {
        // Read from delay line
        uint32_t read_pos = (write_pos + ring_size - delay) % ring_size;
        float delayed = ring[read_pos];

        // Write input + feedback to delay line
        ring[write_pos] = in[i] + delayed * feedback;

        // Output: dry/wet mix
        out[i] = in[i] * dry + delayed * mix;

        // Advance write position
        write_pos = (write_pos + 1) % ring_size;
    }

    state->write_pos = write_pos;
}
