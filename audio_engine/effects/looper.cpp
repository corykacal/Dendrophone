#include "looper.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

LooperState* looper_state_create(float speed, float feedback, float mix, float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    LooperState* state = static_cast<LooperState*>(
        aligned_alloc(64, align_up(sizeof(LooperState))));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;

    // Allocate 60 second buffer
    state->buffer_size = static_cast<uint32_t>(sample_rate * 60.0f);
    state->buffer = static_cast<float*>(
        aligned_alloc(64, align_up(state->buffer_size * sizeof(float))));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }
    memset(state->buffer, 0, state->buffer_size * sizeof(float));

    state->loop_length = 0;
    state->write_pos = 0;
    state->read_pos = 0;
    state->mode = LooperState::Mode::Bypass;
    state->loop_recorded = false;

    // Initialize parameters
    state->speed.base = speed;
    state->speed.smoothed = speed;
    state->speed.set_smoothing(20.0f, sample_rate);

    state->feedback.base = feedback;
    state->feedback.smoothed = feedback;
    state->feedback.smooth_coeff = 0.0f;

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    // Initialize triggers
    state->record_trigger.base = 0.0f;
    state->record_trigger.smoothed = 0.0f;
    state->record_trigger.smooth_coeff = 0.0f;

    state->play_trigger.base = 0.0f;
    state->play_trigger.smoothed = 0.0f;
    state->play_trigger.smooth_coeff = 0.0f;

    state->overdub_trigger.base = 0.0f;
    state->overdub_trigger.smoothed = 0.0f;
    state->overdub_trigger.smooth_coeff = 0.0f;

    state->clear_trigger.base = 0.0f;
    state->clear_trigger.smoothed = 0.0f;
    state->clear_trigger.smooth_coeff = 0.0f;

    return state;
}

void looper_state_destroy(LooperState* state) {
    if (!state) return;
    if (state->buffer) free(state->buffer);
    free(state);
}

void looper_op(DSPBlock& b, float* buffers, int n) {
    LooperState* s = static_cast<LooperState*>(b.state);
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
    const float feedback = std::clamp(s->feedback.get(), 0.0f, 1.0f);
    float speed = std::clamp(s->speed.get_smoothed(), 0.25f, 4.0f);

    // Check for mode changes via triggers
    float record_trig = s->record_trigger.get();
    float play_trig = s->play_trigger.get();
    float overdub_trig = s->overdub_trigger.get();
    float clear_trig = s->clear_trigger.get();

    // Handle clear trigger
    if (clear_trig > 0.5f) {
        memset(s->buffer, 0, s->buffer_size * sizeof(float));
        s->loop_length = 0;
        s->write_pos = 0;
        s->read_pos = 0;
        s->loop_recorded = false;
        s->mode = LooperState::Mode::Bypass;
    }

    // Handle mode triggers (record takes priority)
    if (record_trig > 0.5f && s->mode != LooperState::Mode::Recording) {
        s->mode = LooperState::Mode::Recording;
        s->write_pos = 0;
        s->loop_length = 0;
        s->loop_recorded = false;
    } else if (play_trig > 0.5f && s->loop_recorded) {
        s->mode = LooperState::Mode::Playing;
        s->read_pos = 0;
    } else if (overdub_trig > 0.5f && s->loop_recorded) {
        s->mode = LooperState::Mode::Overdubbing;
        s->read_pos = 0;
    }

    // If recording and another record trigger comes in, stop recording and start playback
    static float prev_record_trig = 0.0f;
    if (s->mode == LooperState::Mode::Recording &&
        record_trig > 0.5f && prev_record_trig < 0.5f && s->write_pos > 0) {
        s->loop_length = s->write_pos;
        s->loop_recorded = true;
        s->mode = LooperState::Mode::Playing;
        s->read_pos = 0;
    }
    prev_record_trig = record_trig;

    // Process based on mode
    for (int i = 0; i < n; i++) {
        float output_sample = in[i];  // Default: bypass

        switch (s->mode) {
            case LooperState::Mode::Bypass:
                // Just pass through
                break;

            case LooperState::Mode::Recording:
                // Record into buffer
                if (s->write_pos < s->buffer_size) {
                    s->buffer[s->write_pos] = in[i];
                    s->write_pos++;
                } else {
                    // Buffer full, stop recording and loop
                    s->loop_length = s->write_pos;
                    s->loop_recorded = true;
                    s->mode = LooperState::Mode::Playing;
                    s->read_pos = 0;
                }
                break;

            case LooperState::Mode::Playing:
                if (s->loop_recorded && s->loop_length > 0) {
                    // Read from loop with speed control
                    uint32_t read_idx = static_cast<uint32_t>(s->read_pos) % s->loop_length;
                    uint32_t read_idx_next = (read_idx + 1) % s->loop_length;
                    float frac = s->read_pos - floorf(s->read_pos);

                    float sample_a = s->buffer[read_idx];
                    float sample_b = s->buffer[read_idx_next];
                    output_sample = sample_a * (1.0f - frac) + sample_b * frac;

                    // Advance read position by speed
                    s->read_pos += speed;
                    if (s->read_pos >= s->loop_length) {
                        s->read_pos -= s->loop_length;
                    }
                }
                break;

            case LooperState::Mode::Overdubbing:
                if (s->loop_recorded && s->loop_length > 0) {
                    // Read from loop
                    uint32_t read_idx = static_cast<uint32_t>(s->read_pos) % s->loop_length;
                    uint32_t read_idx_next = (read_idx + 1) % s->loop_length;
                    float frac = s->read_pos - floorf(s->read_pos);

                    float sample_a = s->buffer[read_idx];
                    float sample_b = s->buffer[read_idx_next];
                    float loop_sample = sample_a * (1.0f - frac) + sample_b * frac;

                    output_sample = loop_sample;

                    // Write back with feedback (overdub)
                    uint32_t write_idx = static_cast<uint32_t>(s->read_pos) % s->loop_length;
                    s->buffer[write_idx] = loop_sample * feedback + in[i] * (1.0f - feedback);

                    // Advance read position
                    s->read_pos += speed;
                    if (s->read_pos >= s->loop_length) {
                        s->read_pos -= s->loop_length;
                    }
                }
                break;
        }

        // Mix dry/wet
        out[i] = in[i] * dry + output_sample * mix;
    }
}
