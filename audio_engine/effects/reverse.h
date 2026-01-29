#ifndef REVERSE_H
#define REVERSE_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Reverse playback effect with fixed-length buffer and crossfading
struct ReverseState {
    float* buffer;              // Ring buffer for recording
    uint32_t buffer_size;       // Current active buffer size in samples
    uint32_t max_buffer_size;   // Maximum allocation (for dynamic sizing)

    uint32_t write_pos;         // Write head position (advances forward)
    float read_pos;             // Read head position (float for interpolation)

    // Crossfading state
    float crossfade_pos;        // Position within crossfade window (0.0 - 1.0)
    uint32_t crossfade_samples; // Crossfade window size in samples
    float prev_sample;          // Last sample from previous window (for xfade)

    float sample_rate;

    // Modulatable parameters
    Param buffer_time_ms;       // Reverse buffer length in milliseconds (50.0 - 2000.0)
    Param mix;                  // Dry/wet mix (0.0 - 1.0)
    Param enabled;              // Reverse on/off (0.0 = forward, 1.0 = reverse)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize reverse state
ReverseState* reverse_state_create(float buffer_time_ms,
                                     float mix,
                                     float enabled,
                                     float sample_rate);

// Control thread: free reverse state
void reverse_state_destroy(ReverseState* state);

// Audio thread: process reverse effect
void reverse_op(DSPBlock& b, float* buffers, int n);

#endif // REVERSE_H
