#include "pitch_shift.h"
#include <cstdlib>
#include <cstring>
#include <cmath>

PitchShiftState* pitch_shift_state_create(float pitch, float mix, float sample_rate) {
    PitchShiftState* state = (PitchShiftState*)aligned_alloc(64, sizeof(PitchShiftState));
    if (!state) return nullptr;

    // Initialize all fields
    state->buffer = nullptr;
    state->buffer_size = 0;
    state->write_pos = 0;
    state->read_pos_a = 0.0f;
    state->read_pos_b = 0.0f;
    state->crossfade = 0.0f;
    state->sample_rate = sample_rate;

    // Buffer size: 200ms to allow for pitch shifts and windowing
    state->buffer_size = (uint32_t)(sample_rate * 0.2f);
    state->buffer = (float*)calloc(state->buffer_size, sizeof(float));
    state->write_pos = 0;

    // Initialize read positions at different points in the buffer
    float window_samples = (PitchShiftState::WINDOW_MS / 1000.0f) * sample_rate;
    state->read_pos_a = state->buffer_size / 2.0f;
    state->read_pos_b = state->read_pos_a + window_samples;
    state->crossfade = 0.0f;

    // Initialize parameters
    state->pitch.base = pitch;
    state->pitch.smoothed = pitch;
    state->pitch.smooth_coeff = 0.001f;  // Smooth pitch changes

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.01f;

    return state;
}

void pitch_shift_state_destroy(PitchShiftState* state) {
    if (!state) return;
    free(state->buffer);
    free(state);
}

// Linear interpolation for reading from delay buffer
static inline float read_buffer_interpolated(float* buffer, uint32_t size, float pos) {
    uint32_t idx0 = (uint32_t)pos % size;
    uint32_t idx1 = (idx0 + 1) % size;
    float frac = pos - floorf(pos);
    return buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;
}

void pitch_shift_op(DSPBlock& b, float* buffers, int n) {
    PitchShiftState* s = (PitchShiftState*)b.state;
    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    // Get smoothed parameters
    float pitch_semitones = s->pitch.get_smoothed();
    float mix = s->mix.get_smoothed();

    // Clamp parameters
    pitch_semitones = fminf(fmaxf(pitch_semitones, -12.0f), 12.0f);
    mix = fminf(fmaxf(mix, 0.0f), 1.0f);

    // Convert semitones to playback rate: 2^(semitones/12)
    float pitch_ratio = powf(2.0f, pitch_semitones / 12.0f);

    // Window size in samples
    float window_samples = (PitchShiftState::WINDOW_MS / 1000.0f) * s->sample_rate;
    float crossfade_inc = 1.0f / window_samples;

    for (int i = 0; i < n; i++) {
        // Write input to buffer
        s->buffer[s->write_pos] = in[i];

        // Read from both heads with interpolation
        float sample_a = read_buffer_interpolated(s->buffer, s->buffer_size, s->read_pos_a);
        float sample_b = read_buffer_interpolated(s->buffer, s->buffer_size, s->read_pos_b);

        // Crossfade between the two heads
        float shifted = sample_a * (1.0f - s->crossfade) + sample_b * s->crossfade;

        // Advance read positions at modified rate
        s->read_pos_a += pitch_ratio;
        s->read_pos_b += pitch_ratio;

        // Wrap read positions
        if (s->read_pos_a >= s->buffer_size) s->read_pos_a -= s->buffer_size;
        if (s->read_pos_b >= s->buffer_size) s->read_pos_b -= s->buffer_size;

        // Update crossfade
        s->crossfade += crossfade_inc;

        // When crossfade completes, swap heads
        if (s->crossfade >= 1.0f) {
            s->crossfade = 0.0f;
            s->read_pos_a = s->read_pos_b;
            s->read_pos_b = s->read_pos_a + window_samples;
            if (s->read_pos_b >= s->buffer_size) s->read_pos_b -= s->buffer_size;
        }

        // Advance write position
        s->write_pos = (s->write_pos + 1) % s->buffer_size;

        // Mix dry/wet
        out[i] = in[i] * (1.0f - mix) + shifted * mix;
    }
}
