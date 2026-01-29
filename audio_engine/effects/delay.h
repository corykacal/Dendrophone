#pragma once

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Delay state - ring buffer for delay effect
struct DelayState {
    float* buffer;          // Ring buffer (preallocated)
    uint32_t buffer_size;   // Total ring buffer size in samples
    uint32_t write_pos;     // Current write position
    uint32_t max_delay_samples;  // Maximum delay in samples
    float sample_rate;      // For converting time_ms modulation

    // Modulatable parameters
    Param time_ms;          // Delay time in milliseconds
    Param feedback;         // Feedback amount (0.0 - 1.0)
    Param mix;              // Wet/dry mix (0.0 = dry, 1.0 = wet)
};

// Create delay state - call from control thread only
// max_delay_samples: maximum delay buffer size
// delay_samples: initial delay (must be <= max_delay_samples)
// feedback: amount of output fed back to input (0.0 - 1.0)
// mix: wet/dry mix (0.0 = all dry, 1.0 = all wet)
// sample_rate: audio sample rate for time conversions
DelayState* delay_state_create(uint32_t max_delay_samples,
                                uint32_t delay_samples,
                                float feedback,
                                float mix,
                                float sample_rate);

// Destroy delay state - call from control thread only
void delay_state_destroy(DelayState* state);

// RT-safe delay processing - use as DSPBlock::fn
void delay_op(DSPBlock& b, float* buffers, int n);
