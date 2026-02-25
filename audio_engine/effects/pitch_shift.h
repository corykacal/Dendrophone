#ifndef PITCH_SHIFT_H
#define PITCH_SHIFT_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Pitch shifter using WSOLA (Waveform Similarity Overlap-Add).
// Two-grain overlap-add with per-grain WSOLA anchor search eliminates
// the ~16.7 Hz tremolo artifact of anchored OLA.
//
// Teensy 4.1 compatible: power-of-2 buffer (& mask instead of %), magic-circle
// Hann oscillator (no cosf in hot path), integer grain counters.
struct PitchShiftState {
    float*   buffer;
    uint32_t buffer_size;
    uint32_t buf_mask;     // buffer_size - 1; enables & mask for circular indexing
    uint32_t write_pos;
    float    sample_rate;

    // Two grains for overlap-add pitch shifting
    float    read_pos[2];       // Current read position in circular buffer
    uint32_t grain_samples[2];  // Integer grain counter (0 .. grain_size-1)
    uint32_t grain_size;        // In samples (GRAIN_MS * sample_rate / 1000)
    uint32_t read_delay;        // Lookback from write_pos in samples

    // Magic-circle Hann oscillator — replaces cosf() every sample with 4 FMAs
    float hann_cos[2];   // Per-grain cosine state
    float hann_sin[2];   // Per-grain sine state
    float hann_cos_inc;  // cos(2π / grain_size), precomputed once
    float hann_sin_inc;  // sin(2π / grain_size), precomputed once

    // WSOLA search parameters
    uint32_t search_delta;  // ± samples around nominal anchor (~5 ms)
    uint32_t corr_len;      // Correlation window length (grain_size / CORR_LEN_DIV)
    uint32_t corr_stride;   // Downsampling factor (evaluate every Nth sample)

    static constexpr float GRAIN_MS        = 180.0f;
    static constexpr float READ_DELAY_MS   = 180.0f;
    static constexpr float SEARCH_DELTA_MS = 40.0f;
    static constexpr int   CORR_LEN_DIV    = 4;
    static constexpr int   CORR_STRIDE     = 2;

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
