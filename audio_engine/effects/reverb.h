#ifndef REVERB_H
#define REVERB_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Schroeder reverb implementation with comb and allpass filters
struct ReverbState {
    // Comb filter delays (parallel)
    static constexpr int NUM_COMBS = 4;
    float* comb_buffers[NUM_COMBS];
    uint32_t comb_sizes[NUM_COMBS];
    uint32_t comb_positions[NUM_COMBS];

    // Allpass filter delays (series)
    static constexpr int NUM_ALLPASSES = 2;
    float* allpass_buffers[NUM_ALLPASSES];
    uint32_t allpass_sizes[NUM_ALLPASSES];
    uint32_t allpass_positions[NUM_ALLPASSES];

    float sample_rate;

    // Modulatable parameters
    Param decay;      // 0.0 to 1.0 (decay time)
    Param mix;        // 0.0 to 1.0 (dry/wet)
    Param room_size;  // 0.0 to 1.0 (scales delay times)
};

// Control thread: allocate and initialize reverb state
ReverbState* reverb_state_create(float decay, float mix, float room_size, float sample_rate);

// Control thread: free reverb state
void reverb_state_destroy(ReverbState* state);

// Audio thread: process reverb
void reverb_op(DSPBlock& b, float* buffers, int n);

#endif // REVERB_H
