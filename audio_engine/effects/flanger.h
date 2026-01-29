#ifndef FLANGER_H
#define FLANGER_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Flanger effect - short modulated delay with feedback
struct FlangerState {
    float* buffer;              // Delay buffer
    uint32_t buffer_size;       // Buffer size in samples
    uint32_t write_pos;         // Write position

    float lfo_phase;            // LFO phase (0 to 2π)
    float sample_rate;

    // Modulatable parameters
    Param rate_hz;              // LFO rate in Hz (0.1 - 10.0)
    Param depth;                // Delay modulation depth (0.0 - 1.0)
    Param feedback;             // Feedback amount (-0.95 - 0.95)
    Param delay_ms;             // Center delay time in ms (0.1 - 10.0)
    Param mix;                  // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize flanger state
FlangerState* flanger_state_create(float rate_hz,
                                     float depth,
                                     float feedback,
                                     float delay_ms,
                                     float mix,
                                     float sample_rate);

// Control thread: free flanger state
void flanger_state_destroy(FlangerState* state);

// Audio thread: process flanger effect
void flanger_op(DSPBlock& b, float* buffers, int n);

#endif // FLANGER_H
