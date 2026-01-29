#ifndef PITCH_SHIFT_H
#define PITCH_SHIFT_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Simple pitch shifter using dual delay lines with crossfading
struct PitchShiftState {
    float* buffer;
    uint32_t buffer_size;
    uint32_t write_pos;

    // Two read heads for crossfading
    float read_pos_a;
    float read_pos_b;
    float crossfade;  // 0.0 to 1.0

    float sample_rate;
    static constexpr float WINDOW_MS = 50.0f;  // Crossfade window duration

    // Modulatable parameters
    Param pitch;      // Pitch shift in semitones (-12.0 to +12.0)
    Param mix;        // Dry/wet mix (0.0 to 1.0)
};

// Control thread: allocate and initialize pitch shift state
PitchShiftState* pitch_shift_state_create(float pitch, float mix, float sample_rate);

// Control thread: free pitch shift state
void pitch_shift_state_destroy(PitchShiftState* state);

// Audio thread: process pitch shift
void pitch_shift_op(DSPBlock& b, float* buffers, int n);

#endif // PITCH_SHIFT_H
