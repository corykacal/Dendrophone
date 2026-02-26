#include "reverse.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

ReverseState* reverse_state_create(float buffer_time_ms,
                                     float mix,
                                     float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    ReverseState* state = static_cast<ReverseState*>(
        aligned_alloc(64, align_up(sizeof(ReverseState))));
    if (!state) return nullptr;

    // Calculate maximum buffer size (2 seconds max)
    state->sample_rate = sample_rate;
    state->max_buffer_size = static_cast<uint32_t>(sample_rate * 2.0f);

    // Allocate buffer
    state->buffer = static_cast<float*>(
        aligned_alloc(64, align_up(state->max_buffer_size * sizeof(float))));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }

    memset(state->buffer, 0, state->max_buffer_size * sizeof(float));

    // Initialize buffer size from parameter
    uint32_t initial_buffer_size = static_cast<uint32_t>(
        buffer_time_ms * sample_rate / 1000.0f);
    state->buffer_size = std::clamp(initial_buffer_size,
                                      960u,  // 20ms at 48kHz minimum
                                      state->max_buffer_size);

    state->write_pos = 0;
    state->read_pos = 0.0f;

    // Initialize crossfade (10ms window)
    state->crossfade_samples = static_cast<uint32_t>(sample_rate * 0.01f);
    state->crossfade_pos = 1.0f;  // Start with crossfade complete
    state->prev_sample = 0.0f;

    // Initialize buffer_time_ms parameter (with smoothing)
    state->buffer_time_ms.base = buffer_time_ms;
    state->buffer_time_ms.mod = 0.0f;
    state->buffer_time_ms.smoothed = buffer_time_ms;
    state->buffer_time_ms.set_smoothing(50.0f, sample_rate);  // 50ms smoothing

    // Initialize mix parameter (with smoothing for smooth transitions)
    state->mix.base = mix;
    state->mix.mod = 0.0f;
    state->mix.smoothed = mix;
    state->mix.set_smoothing(10.0f, sample_rate);  // 10ms smoothing

    return state;
}

void reverse_state_destroy(ReverseState* state) {
    if (!state) return;
    if (state->buffer) free(state->buffer);
    free(state);
}

void reverse_op(DSPBlock& b, float* buffers, int n) {
    ReverseState* s = static_cast<ReverseState*>(b.state);
    if (!s) {
        // Passthrough fallback if no state
        float* in = &buffers[b.in_a * n];
        float* out = &buffers[b.out * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    const float mix = std::clamp(s->mix.get_smoothed(), 0.0f, 1.0f);
    const float dry = 1.0f - mix;

    // Get buffer time and convert to samples
    float buffer_ms = s->buffer_time_ms.get_smoothed();
    uint32_t active_buffer_size = static_cast<uint32_t>(
        buffer_ms * s->sample_rate / 1000.0f);
    active_buffer_size = std::clamp(active_buffer_size,
                                      960u,  // 20ms at 48kHz minimum
                                      s->max_buffer_size);

    // Update buffer size if changed
    if (active_buffer_size != s->buffer_size) {
        s->buffer_size = active_buffer_size;
        // Reset read position to avoid reading outside new buffer
        s->read_pos = static_cast<float>(s->write_pos);
        s->crossfade_pos = 0.0f;  // Restart crossfade
    }

    for (int i = 0; i < n; i++) {
        // Write input to buffer
        s->buffer[s->write_pos] = in[i];

        // Read from buffer (backward) with linear interpolation
        uint32_t read_idx = static_cast<uint32_t>(s->read_pos) % s->max_buffer_size;
        float frac = s->read_pos - floorf(s->read_pos);

        // Calculate next sample index (backward)
        uint32_t read_idx_next = read_idx > 0 ? read_idx - 1 : s->buffer_size - 1;
        read_idx_next = read_idx_next % s->max_buffer_size;

        float sample_a = s->buffer[read_idx];
        float sample_b = s->buffer[read_idx_next];
        float reversed = sample_a * (1.0f - frac) + sample_b * frac;

        // Apply crossfade if in crossfade window
        if (s->crossfade_pos < 1.0f) {
            // Cosine crossfade for smooth transition
            float fade = 0.5f - 0.5f * cosf(s->crossfade_pos * M_PI);
            reversed = s->prev_sample * (1.0f - fade) + reversed * fade;
            s->crossfade_pos += 1.0f / s->crossfade_samples;
        }

        // Decrement read position (read backward)
        s->read_pos -= 1.0f;

        // If we've read the entire buffer, restart
        if (s->read_pos < 0.0f) {
            s->prev_sample = reversed;  // Save for crossfade
            s->read_pos = static_cast<float>(s->write_pos);
            s->crossfade_pos = 0.0f;
        }

        // Mix dry/wet (mix=0 → forward, mix=1 → fully reversed)
        out[i] = in[i] * dry + reversed * mix;

        // Advance write position
        s->write_pos = (s->write_pos + 1) % s->max_buffer_size;
    }
}
