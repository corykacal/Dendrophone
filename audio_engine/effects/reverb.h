#ifndef REVERB_H
#define REVERB_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Schroeder reverb implementation with comb and allpass filters
struct ReverbState {
    // Comb filter delays (parallel)
    static constexpr int NUM_COMBS = 8;
    float* comb_buffers[NUM_COMBS];
    uint32_t comb_sizes[NUM_COMBS];
    uint32_t comb_positions[NUM_COMBS];
    float comb_filter_state[NUM_COMBS];  // one-pole LPF state per comb (damping)

    // Allpass filter delays (series)
    static constexpr int NUM_ALLPASSES = 4;
    float* allpass_buffers[NUM_ALLPASSES];
    uint32_t allpass_sizes[NUM_ALLPASSES];
    uint32_t allpass_positions[NUM_ALLPASSES];

    // Pre-delay buffer
    float* pre_delay_buffer;
    uint32_t pre_delay_buffer_size;  // max 50ms
    uint32_t pre_delay_write_pos;

    float sample_rate;

    // Modulatable parameters
    Param decay;        // 0.0 to 1.0 (decay time)
    Param mix;          // 0.0 to 1.0 (dry/wet)
    Param room_size;    // 0.0 to 1.0 (scales delay times)
    Param damping;      // 0.0 to 1.0 (HF absorption per reflection)
    Param pre_delay_ms; // 0.0 to 50.0 (ms before reverb tail begins)
};

// Control thread: allocate and initialize reverb state
ReverbState* reverb_state_create(float decay, float mix, float room_size,
                                  float damping, float pre_delay_ms,
                                  float sample_rate);

// Control thread: free reverb state
void reverb_state_destroy(ReverbState* state);

// Audio thread: process reverb
void reverb_op(DSPBlock& b, float* buffers, int n);

#endif // REVERB_H
