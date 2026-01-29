#pragma once

#include "dsp_block.h"
#include <cstdint>
#include <cstring>

struct DSPProgram {
    float* buffers;
    DSPBlock* blocks;
    uint32_t num_blocks;
    uint32_t num_buffers;
    uint32_t buffer_size;      // Samples per buffer (frames_per_buffer, NOT frames*channels)
    uint32_t input_buffer;     // First input buffer index
    uint32_t output_buffer;    // First output buffer index
    uint32_t num_channels;     // Number of audio channels (e.g., 2 for stereo)

    // Process interleaved audio
    // in/out: interleaved audio [L0,R0,L1,R1,...] with frames*channels samples
    // frames: number of audio frames (NOT samples)
    inline void process(float* in, float* out, int frames) {
        // Deinterleave input into separate channel buffers
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            float* dst = &buffers[(input_buffer + ch) * buffer_size];
            for (int i = 0; i < frames; i++) {
                dst[i] = in[i * num_channels + ch];
            }
        }

        // Run DSP blocks (each buffer holds 'frames' samples)
        for (uint32_t i = 0; i < num_blocks; i++) {
            blocks[i].fn(blocks[i], buffers, frames);
        }

        // Interleave output from separate channel buffers
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            float* src = &buffers[(output_buffer + ch) * buffer_size];
            for (int i = 0; i < frames; i++) {
                out[i * num_channels + ch] = src[i];
            }
        }
    }
};
