#pragma once

#include "dsp_block.h"
#include <cstdint>
#include <cstring>

struct DSPProgram {
    float* buffers;
    DSPBlock* blocks;
    uint32_t num_blocks;
    uint32_t num_buffers;
    uint32_t buffer_size;
    uint32_t input_buffer;
    uint32_t output_buffer;

    inline void process(float* in, float* out, int n) {
        memcpy(&buffers[input_buffer * buffer_size], in, n * sizeof(float));

        for (uint32_t i = 0; i < num_blocks; i++) {
            blocks[i].fn(blocks[i], buffers, n);
        }

        memcpy(out, &buffers[output_buffer * buffer_size], n * sizeof(float));
    }
};
