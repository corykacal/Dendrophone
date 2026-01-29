#pragma once

#include "../core/dsp_block.h"
#include <cstdint>

struct DelayState;

// Create delay state - call from control thread only
// max_delay_samples: maximum delay buffer size
// delay_samples: initial delay (must be <= max_delay_samples)
// feedback: amount of output fed back to input (0.0 - 1.0)
// mix: wet/dry mix (0.0 = all dry, 1.0 = all wet)
DelayState* delay_state_create(uint32_t max_delay_samples,
                                uint32_t delay_samples,
                                float feedback = 0.0f,
                                float mix = 1.0f);

// Destroy delay state - call from control thread only
void delay_state_destroy(DelayState* state);

// RT-safe delay processing - use as DSPBlock::fn
void delay_op(DSPBlock& b, float* buffers, int n);
