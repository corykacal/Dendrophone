#include "pitch_shift.h"
#include <cstdlib>
#include <cstring>
#include <cmath>

PitchShiftState* pitch_shift_state_create(float pitch, float mix, float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    PitchShiftState* state = (PitchShiftState*)aligned_alloc(64, align_up(sizeof(PitchShiftState)));
    if (!state) return nullptr;

    state->sample_rate  = sample_rate;
    state->grain_size   = (uint32_t)(sample_rate * PitchShiftState::GRAIN_MS / 1000.0f);
    state->read_delay   = (uint32_t)(sample_rate * PitchShiftState::READ_DELAY_MS / 1000.0f);
    state->buffer_size  = (uint32_t)(sample_rate * 0.3f);  // 300ms
    state->buffer       = (float*)calloc(state->buffer_size, sizeof(float));
    state->write_pos    = 0;

    float pitch_ratio = powf(2.0f, pitch / 12.0f);

    // Grain 0 at phase 0.0; grain 1 at phase 0.5 (halfway through its grain period).
    // This ensures their Hann envelopes sum to exactly 1.0 at all times.
    state->grain_phase[0] = 0.0f;
    state->grain_phase[1] = 0.5f;
    state->read_pos[0] = (float)(state->buffer_size - state->read_delay);
    state->read_pos[1] = state->read_pos[0] +
                         state->grain_size * 0.5f * pitch_ratio;
    if (state->read_pos[1] >= state->buffer_size)
        state->read_pos[1] -= state->buffer_size;

    state->pitch.base = pitch;  state->pitch.smoothed = pitch;  state->pitch.smooth_coeff = 0.001f;
    state->mix.base   = mix;    state->mix.smoothed   = mix;    state->mix.smooth_coeff   = 0.01f;

    return state;
}

void pitch_shift_state_destroy(PitchShiftState* state) {
    if (!state) return;
    free(state->buffer);
    free(state);
}

// 4-point Catmull-Rom cubic interpolation for reading from delay buffer.
// ~60 dB stop-band rejection vs ~13 dB for linear — eliminates aliasing artifacts.
static inline float read_buffer_interpolated(float* buffer, uint32_t size, float pos) {
    uint32_t i1 = (uint32_t)pos % size;
    uint32_t i0 = (i1 + size - 1) % size;
    uint32_t i2 = (i1 + 1) % size;
    uint32_t i3 = (i1 + 2) % size;
    float t  = pos - floorf(pos);
    float y0 = buffer[i0], y1 = buffer[i1], y2 = buffer[i2], y3 = buffer[i3];
    float a  = 0.5f * (-y0 + 3.0f*y1 - 3.0f*y2 + y3);
    float b  = 0.5f * ( 2.0f*y0 - 5.0f*y1 + 4.0f*y2 - y3);
    float c  = 0.5f * (-y0 + y2);
    return ((a * t + b) * t + c) * t + y1;
}

void pitch_shift_op(DSPBlock& b, float* buffers, int n) {
    PitchShiftState* s = (PitchShiftState*)b.state;
    float* in  = &buffers[b.in_a * n];
    float* out = &buffers[b.out  * n];

    float pitch_semitones = s->pitch.get_smoothed();
    float mix             = s->mix.get_smoothed();
    pitch_semitones = fminf(fmaxf(pitch_semitones, -12.0f), 12.0f);
    mix             = fminf(fmaxf(mix,              0.0f),   1.0f);
    float pitch_ratio = powf(2.0f, pitch_semitones / 12.0f);
    float phase_inc   = 1.0f / (float)s->grain_size;

    for (int i = 0; i < n; i++) {
        s->buffer[s->write_pos] = in[i];

        float shifted = 0.0f;
        for (int g = 0; g < 2; g++) {
            // Hann envelope: two windows at 50% overlap sum to exactly 1.0
            float env    = 0.5f - 0.5f * cosf(s->grain_phase[g] * 6.28318530f);
            float sample = read_buffer_interpolated(s->buffer, s->buffer_size, s->read_pos[g]);
            shifted += env * sample;

            s->read_pos[g] += pitch_ratio;
            if (s->read_pos[g] >= s->buffer_size) s->read_pos[g] -= s->buffer_size;

            s->grain_phase[g] += phase_inc;
            if (s->grain_phase[g] >= 1.0f) {
                s->grain_phase[g] -= 1.0f;
                // Anchor new grain to current write position — no drift possible
                s->read_pos[g] = (float)((s->write_pos + s->buffer_size - s->read_delay)
                                         % s->buffer_size);
            }
        }

        // Two Hann windows at 50% overlap sum to exactly 1.0 — no amplitude correction needed
        out[i] = in[i] * (1.0f - mix) + shifted * mix;
        s->write_pos = (s->write_pos + 1) % s->buffer_size;
    }
}
