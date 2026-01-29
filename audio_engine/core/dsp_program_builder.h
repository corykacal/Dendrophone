#pragma once

#include "dsp_program.h"
#include "dsp_block.h"
#include <vector>
#include <cstdlib>
#include <cstring>

// DSPProgramBuilder: Control-thread safe construction of DSPPrograms
// Creates immutable, preallocated programs for hot-swap into audio thread
class DSPProgramBuilder {
public:
    explicit DSPProgramBuilder(uint32_t buffer_size, uint32_t num_buffers)
        : buffer_size_(buffer_size), num_buffers_(num_buffers) {
    }

    // Add a DSP block to the program
    void add_block(void (*fn)(DSPBlock&, float*, int),
                   uint32_t in_a, uint32_t in_b, uint32_t out,
                   void* state = nullptr) {
        DSPBlock block;
        block.fn = fn;
        block.in_a = in_a;
        block.in_b = in_b;
        block.out = out;
        block.state = state;
        blocks_.push_back(block);
    }

    void set_input_buffer(uint32_t idx) { input_buffer_ = idx; }
    void set_output_buffer(uint32_t idx) { output_buffer_ = idx; }

    // Build the final program - caller takes ownership
    // Returns nullptr on failure
    DSPProgram* build() {
        // Allocate program struct
        DSPProgram* program = static_cast<DSPProgram*>(
            aligned_alloc(64, sizeof(DSPProgram)));
        if (!program) return nullptr;

        // Allocate buffers (cache-line aligned for performance)
        size_t buffer_bytes = buffer_size_ * num_buffers_ * sizeof(float);
        program->buffers = static_cast<float*>(aligned_alloc(64, buffer_bytes));
        if (!program->buffers) {
            free(program);
            return nullptr;
        }
        memset(program->buffers, 0, buffer_bytes);

        // Allocate and copy blocks
        if (!blocks_.empty()) {
            size_t blocks_bytes = blocks_.size() * sizeof(DSPBlock);
            program->blocks = static_cast<DSPBlock*>(aligned_alloc(64, blocks_bytes));
            if (!program->blocks) {
                free(program->buffers);
                free(program);
                return nullptr;
            }
            memcpy(program->blocks, blocks_.data(), blocks_bytes);
        } else {
            program->blocks = nullptr;
        }

        program->num_blocks = static_cast<uint32_t>(blocks_.size());
        program->num_buffers = num_buffers_;
        program->buffer_size = buffer_size_;
        program->input_buffer = input_buffer_;
        program->output_buffer = output_buffer_;

        return program;
    }

    // Free a program created by build()
    static void destroy(DSPProgram* program) {
        if (!program) return;

        // Note: state pointers in blocks must be freed separately by caller
        // if they were heap-allocated

        if (program->blocks) {
            free(program->blocks);
        }
        if (program->buffers) {
            free(program->buffers);
        }
        free(program);
    }

private:
    uint32_t buffer_size_;
    uint32_t num_buffers_;
    uint32_t input_buffer_ = 0;
    uint32_t output_buffer_ = 1;
    std::vector<DSPBlock> blocks_;
};
