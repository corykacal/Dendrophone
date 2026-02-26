#include "glitch.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

// Simple random number generator (LCG)
static inline float random_float_glitch(uint32_t& seed) {
    seed = seed * 1103515245 + 12345;
    return static_cast<float>((seed / 65536) % 32768) / 32768.0f;
}

GlitchState* glitch_state_create(float freeze,
                                  float stutter_rate_hz,
                                  float capture_size_ms,
                                  float speed,
                                  float randomize,
                                  float mix,
                                  float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    GlitchState* state = static_cast<GlitchState*>(
        aligned_alloc(64, align_up(sizeof(GlitchState))));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;

    // Allocate buffer (1 second max)
    state->buffer_size = static_cast<uint32_t>(sample_rate);
    state->buffer = static_cast<float*>(
        aligned_alloc(64, align_up(state->buffer_size * sizeof(float))));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }
    memset(state->buffer, 0, state->buffer_size * sizeof(float));

    state->capture_length = static_cast<uint32_t>(
        capture_size_ms * sample_rate / 1000.0f);
    state->capture_length = std::clamp(state->capture_length,
                                        480u, state->buffer_size);

    state->write_pos = 0;
    state->read_pos = 0.0f;
    state->frozen = false;
    state->stutter_counter = 0;
    state->random_seed = 54321;
    state->xfade_gain = 1.0f;
    state->xfade_dir = 0;

    // Initialize parameters
    state->freeze.base = freeze;
    state->freeze.smoothed = freeze;
    state->freeze.set_smoothing(10.0f, sample_rate);

    state->stutter_rate_hz.base = stutter_rate_hz;
    state->stutter_rate_hz.smoothed = stutter_rate_hz;
    state->stutter_rate_hz.set_smoothing(50.0f, sample_rate);

    state->capture_size_ms.base = capture_size_ms;
    state->capture_size_ms.smoothed = capture_size_ms;
    state->capture_size_ms.set_smoothing(30.0f, sample_rate);

    state->speed.base = speed;
    state->speed.smoothed = speed;
    state->speed.set_smoothing(20.0f, sample_rate);

    state->randomize.base = randomize;
    state->randomize.smoothed = randomize;
    state->randomize.set_smoothing(20.0f, sample_rate);

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.set_smoothing(20.0f, sample_rate);

    return state;
}

void glitch_state_destroy(GlitchState* state) {
    if (!state) return;
    if (state->buffer) free(state->buffer);
    free(state);
}

void glitch_op(DSPBlock& b, float* buffers, int n) {
    GlitchState* s = static_cast<GlitchState*>(b.state);
    if (!s) {
        float* in = &buffers[b.in_a * n];
        float* out = &buffers[b.out * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    const float mix = std::clamp(s->mix.get_smoothed(), 0.0f, 1.0f);
    const float dry = 1.0f - mix;

    // Get parameters
    float freeze_amt = std::clamp(s->freeze.get_smoothed(), 0.0f, 1.0f);
    float stutter_rate = std::clamp(s->stutter_rate_hz.get_smoothed(), 0.5f, 50.0f);
    float capture_ms = std::clamp(s->capture_size_ms.get_smoothed(), 10.0f, 1000.0f);
    float speed = std::clamp(s->speed.get_smoothed(), 0.25f, 4.0f);
    float randomize = std::clamp(s->randomize.get_smoothed(), 0.0f, 1.0f);

    // Update capture length
    uint32_t new_capture_length = static_cast<uint32_t>(
        capture_ms * s->sample_rate / 1000.0f);
    new_capture_length = std::clamp(new_capture_length, 480u, s->buffer_size);

    if (new_capture_length != s->capture_length) {
        s->capture_length = new_capture_length;
        // Reset positions if buffer size changes significantly
        if (s->read_pos >= s->capture_length) {
            s->read_pos = 0.0f;
        }
    }

    // Calculate stutter interval
    uint32_t stutter_interval = static_cast<uint32_t>(
        s->sample_rate / stutter_rate);
    stutter_interval = std::max(stutter_interval, 100u);

    // Crossfade constants (~1.3 ms, capped at half stutter interval)
    const int32_t XFADE_LEN = 64;
    const int32_t xfade_len = std::min(XFADE_LEN, (int32_t)(stutter_interval / 2));
    const float   xfade_step = (xfade_len > 0) ? 1.0f / (float)xfade_len : 1.0f;

    s->frozen = (freeze_amt > 0.5f);

    for (int i = 0; i < n; i++) {
        // 1. Capture: write live audio into ring buffer (unless frozen)
        if (!s->frozen) {
            s->buffer[s->write_pos] = in[i];
            s->write_pos = (s->write_pos + 1) % s->capture_length;
        }

        // 2. Step crossfade envelope
        if (s->xfade_dir != 0) {
            s->xfade_gain += (float)s->xfade_dir * xfade_step;
            if (s->xfade_gain <= 0.0f) {
                s->xfade_gain = 0.0f;
                // Gain has reached silence — safe to jump read head now
                int32_t base = (int32_t)s->write_pos - (int32_t)stutter_interval;
                float rand_val = (random_float_glitch(s->random_seed) * 2.0f - 1.0f) * randomize;
                int32_t rand_offset = (int32_t)(rand_val * (float)s->capture_length * 0.5f);
                int32_t new_pos = base + rand_offset;
                new_pos = ((new_pos % (int32_t)s->capture_length) + (int32_t)s->capture_length) % (int32_t)s->capture_length;
                s->read_pos = (float)new_pos;

                // Speed jitter
                if (randomize > 0.0f) {
                    float sv = (random_float_glitch(s->random_seed) * 2.0f - 1.0f) * randomize * 0.5f;
                    speed = std::clamp(speed + sv, 0.25f, 4.0f);
                }

                s->xfade_dir = +1;  // Start fade-in
            } else if (s->xfade_gain >= 1.0f) {
                s->xfade_gain = 1.0f;
                s->xfade_dir = 0;   // Done
            }
        }

        // 3. Stutter: begin fade-out on timer (jump happens at gain=0 above)
        if (++s->stutter_counter >= stutter_interval) {
            s->stutter_counter = 0;
            s->xfade_dir = -1;  // Begin fade-out
        }

        // 4. Read from ring buffer with linear interpolation
        uint32_t ri  = (uint32_t)s->read_pos % s->capture_length;
        uint32_t ri2 = (ri + 1) % s->capture_length;
        float frac   = s->read_pos - floorf(s->read_pos);
        float wet    = s->buffer[ri] * (1.0f - frac) + s->buffer[ri2] * frac;

        // Apply crossfade envelope
        wet *= s->xfade_gain;

        // 5. Advance read position at playback speed
        s->read_pos += speed;
        if (s->read_pos >= (float)s->capture_length)
            s->read_pos -= (float)s->capture_length;

        // 6. Mix
        out[i] = in[i] * dry + wet * mix;
    }
}
