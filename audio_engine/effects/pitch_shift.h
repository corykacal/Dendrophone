#ifndef PITCH_SHIFT_H
#define PITCH_SHIFT_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Pitch shifter using two-grain anchored OLA (overlap-add).
// Each grain resets its read position to write_pos - READ_DELAY_MS,
// preventing drift and the repeating-segment artifact of the old design.
struct PitchShiftState {
    float* buffer;
    uint32_t buffer_size;
    uint32_t write_pos;
    float sample_rate;

    // Two grains for overlap-add pitch shifting
    float read_pos[2];     // Current read position in circular buffer
    float grain_phase[2];  // 0.0 → 1.0 per grain period
    uint32_t grain_size;   // In samples (GRAIN_MS * sample_rate / 1000)
    uint32_t read_delay;   // Lookback from write_pos in samples

    static constexpr float GRAIN_MS      = 80.0f;   // Grain duration
    static constexpr float READ_DELAY_MS = 120.0f;  // Lookback from write head

    Param pitch;  // semitones (-12 to +12)
    Param mix;    // dry/wet (0–1)
};

// Control thread: allocate and initialize pitch shift state
PitchShiftState* pitch_shift_state_create(float pitch, float mix, float sample_rate);

// Control thread: free pitch shift state
void pitch_shift_state_destroy(PitchShiftState* state);

// Audio thread: process pitch shift
void pitch_shift_op(DSPBlock& b, float* buffers, int n);

#endif // PITCH_SHIFT_H
