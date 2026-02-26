#ifndef FLANGER_H
#define FLANGER_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Flanger effect - short delay with feedback; modulate delay_ms externally via LFO node
struct FlangerState {
    float* buffer;              // Delay buffer
    uint32_t buffer_size;       // Buffer size in samples
    uint32_t write_pos;         // Write position

    float sample_rate;

    // Modulatable parameters
    Param feedback;             // Feedback amount (-0.95 - 0.95)
    Param delay_ms;             // Delay time in ms (0.1 - 20.0); wire an LFO here
    Param mix;                  // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize flanger state
FlangerState* flanger_state_create(float feedback,
                                     float delay_ms,
                                     float mix,
                                     float sample_rate);

// Control thread: free flanger state
void flanger_state_destroy(FlangerState* state);

// Audio thread: process flanger effect
void flanger_op(DSPBlock& b, float* buffers, int n);

#endif // FLANGER_H
