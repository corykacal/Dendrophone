#include "../core/dsp_block.h"
#include <cstdlib>
#include <cstring>

// Delay effect with ring buffer
// RT-safe: no allocations, no locks during process

struct DelayState {
    float* buffer;          // Ring buffer (preallocated)
    uint32_t buffer_size;   // Total ring buffer size in samples
    uint32_t write_pos;     // Current write position
    uint32_t delay_samples; // Delay length in samples
    float feedback;         // Feedback amount (0.0 - 1.0)
    float mix;              // Wet/dry mix (0.0 = dry, 1.0 = wet)
};

// Create delay state - call from control thread only
inline DelayState* delay_state_create(uint32_t max_delay_samples,
                                       uint32_t delay_samples,
                                       float feedback = 0.0f,
                                       float mix = 1.0f) {
    DelayState* state = static_cast<DelayState*>(
        aligned_alloc(64, sizeof(DelayState)));
    if (!state) return nullptr;

    state->buffer = static_cast<float*>(
        aligned_alloc(64, max_delay_samples * sizeof(float)));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }

    memset(state->buffer, 0, max_delay_samples * sizeof(float));
    state->buffer_size = max_delay_samples;
    state->write_pos = 0;
    state->delay_samples = delay_samples;
    state->feedback = feedback;
    state->mix = mix;

    return state;
}

// Destroy delay state - call from control thread only
inline void delay_state_destroy(DelayState* state) {
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
    const uint32_t delay = state->delay_samples;
    const float feedback = state->feedback;
    const float mix = state->mix;
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
