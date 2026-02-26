#ifndef GAIN_H
#define GAIN_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"

struct GainState {
    float sample_rate;
    Param gain;  // 0.0–4.0, default 1.0
} __attribute__((aligned(64)));

GainState* gain_state_create(float gain, float sample_rate);
void gain_state_destroy(GainState* state);
void gain_op(DSPBlock& b, float* buffers, int n);

#endif // GAIN_H
