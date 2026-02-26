#pragma once

#include "dsp_block.h"
#include <cstring>

// Copy: out = in_a
inline void copy_op(DSPBlock& b, float* buffers, int n) {
    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];
    memcpy(out, in, n * sizeof(float));
}

// Passthrough: alias for copy
inline void passthrough_op(DSPBlock& b, float* buffers, int n) {
    copy_op(b, buffers, n);
}

// Mix: out = in_a + in_b
inline void mix_op(DSPBlock& b, float* buffers, int n) {
    float* in_a = &buffers[b.in_a * n];
    float* in_b = &buffers[b.in_b * n];
    float* out = &buffers[b.out * n];
    for (int i = 0; i < n; i++) {
        out[i] = in_a[i] + in_b[i];
    }
}

// Invert: out = -in_a (phase inversion)
inline void invert_op(DSPBlock& b, float* buffers, int n) {
    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];
    for (int i = 0; i < n; i++) {
        out[i] = -in[i];
    }
}
