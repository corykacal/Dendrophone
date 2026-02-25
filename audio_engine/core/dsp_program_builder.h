#pragma once

#include "dsp_program.h"
#include "dsp_block.h"
#include "dsp_param.h"
#include <vector>
#include <cstdlib>
#include <cstring>

// DSPProgramBuilder: Control-thread safe construction of DSPPrograms
// Creates immutable, preallocated programs for hot-swap into audio thread
class DSPProgramBuilder {
public:
    // buffer_size: samples per buffer (typically = frames per buffer)
    // num_buffers: total number of audio buffers needed
    // num_channels: audio channels (e.g., 2 for stereo)
    explicit DSPProgramBuilder(uint32_t buffer_size, uint32_t num_buffers, uint32_t num_channels = 2)
        : buffer_size_(buffer_size), num_buffers_(num_buffers), num_channels_(num_channels) {
    }

    // Add a control-rate block (LFO, envelope, etc.)
    void add_control_block(void (*fn)(DSPBlock&, float*, int),
                           uint32_t in_a, uint32_t in_b, uint32_t out,
                           void* state = nullptr) {
        DSPBlock block;
        block.fn = fn;
        block.in_a = in_a;
        block.in_b = in_b;
        block.out = out;
        block.state = state;
        control_blocks_.push_back(block);
    }

    // Add an audio-rate block (effects, etc.)
    void add_audio_block(void (*fn)(DSPBlock&, float*, int),
                         uint32_t in_a, uint32_t in_b, uint32_t out,
                         void* state = nullptr) {
        DSPBlock block;
        block.fn = fn;
        block.in_a = in_a;
        block.in_b = in_b;
        block.out = out;
        block.state = state;
        audio_blocks_.push_back(block);
    }

    // Legacy: add block (defaults to audio-rate for backwards compatibility)
    void add_block(void (*fn)(DSPBlock&, float*, int),
                   uint32_t in_a, uint32_t in_b, uint32_t out,
                   void* state = nullptr) {
        add_audio_block(fn, in_a, in_b, out, state);
    }

    // Add a modulation route
    void add_mod_route(uint32_t control_buffer, Param* target, float scale = 1.0f) {
        ModRoute route;
        route.control_buffer = control_buffer;
        route.target = target;
        route.scale = scale;
        mod_routes_.push_back(route);
    }

    void set_input_buffer(uint32_t idx) { input_buffer_ = idx; }
    void set_output_buffer(uint32_t idx) { output_buffer_ = idx; }
    void set_num_control_buffers(uint32_t n) { num_control_buffers_ = n; }

    // Build the final program - caller takes ownership
    // Returns nullptr on failure
    DSPProgram* build() {
        // aligned_alloc requires size to be a multiple of alignment (strictly enforced on macOS)
        auto align_up = [](size_t n) -> size_t {
            return (n + 63) & ~size_t(63);
        };

        // Allocate program struct
        DSPProgram* program = static_cast<DSPProgram*>(
            aligned_alloc(64, align_up(sizeof(DSPProgram))));
        if (!program) return nullptr;
        memset(program, 0, sizeof(DSPProgram));

        // Allocate audio buffers (cache-line aligned for performance)
        size_t buffer_bytes = buffer_size_ * num_buffers_ * sizeof(float);
        program->buffers = static_cast<float*>(aligned_alloc(64, align_up(buffer_bytes)));
        if (!program->buffers) {
            free(program);
            return nullptr;
        }
        memset(program->buffers, 0, buffer_bytes);

        // Allocate control buffers
        if (num_control_buffers_ > 0) {
            size_t ctrl_bytes = num_control_buffers_ * sizeof(float);
            program->control_buffers = static_cast<float*>(aligned_alloc(64, align_up(ctrl_bytes)));
            if (!program->control_buffers) {
                free(program->buffers);
                free(program);
                return nullptr;
            }
            memset(program->control_buffers, 0, ctrl_bytes);
        } else {
            program->control_buffers = nullptr;
        }

        // Allocate and copy control blocks
        if (!control_blocks_.empty()) {
            size_t blocks_bytes = control_blocks_.size() * sizeof(DSPBlock);
            program->control_blocks = static_cast<DSPBlock*>(aligned_alloc(64, align_up(blocks_bytes)));
            if (!program->control_blocks) {
                free(program->control_buffers);
                free(program->buffers);
                free(program);
                return nullptr;
            }
            memcpy(program->control_blocks, control_blocks_.data(), blocks_bytes);
        } else {
            program->control_blocks = nullptr;
        }

        // Allocate and copy audio blocks
        if (!audio_blocks_.empty()) {
            size_t blocks_bytes = audio_blocks_.size() * sizeof(DSPBlock);
            program->audio_blocks = static_cast<DSPBlock*>(aligned_alloc(64, align_up(blocks_bytes)));
            if (!program->audio_blocks) {
                free(program->control_blocks);
                free(program->control_buffers);
                free(program->buffers);
                free(program);
                return nullptr;
            }
            memcpy(program->audio_blocks, audio_blocks_.data(), blocks_bytes);
        } else {
            program->audio_blocks = nullptr;
        }

        // Allocate and copy mod routes
        if (!mod_routes_.empty()) {
            size_t routes_bytes = mod_routes_.size() * sizeof(ModRoute);
            program->mod_routes = static_cast<ModRoute*>(aligned_alloc(64, align_up(routes_bytes)));
            if (!program->mod_routes) {
                free(program->audio_blocks);
                free(program->control_blocks);
                free(program->control_buffers);
                free(program->buffers);
                free(program);
                return nullptr;
            }
            memcpy(program->mod_routes, mod_routes_.data(), routes_bytes);
        } else {
            program->mod_routes = nullptr;
        }

        program->num_buffers = num_buffers_;
        program->buffer_size = buffer_size_;
        program->num_control_buffers = num_control_buffers_;
        program->num_control_blocks = static_cast<uint32_t>(control_blocks_.size());
        program->num_audio_blocks = static_cast<uint32_t>(audio_blocks_.size());
        program->num_mod_routes = static_cast<uint32_t>(mod_routes_.size());
        program->input_buffer = input_buffer_;
        program->output_buffer = output_buffer_;
        program->num_channels = num_channels_;

        return program;
    }

    // Free a program created by build()
    static void destroy(DSPProgram* program) {
        if (!program) return;

        // Note: state pointers in blocks must be freed separately by caller
        // if they were heap-allocated

        if (program->mod_routes) free(program->mod_routes);
        if (program->audio_blocks) free(program->audio_blocks);
        if (program->control_blocks) free(program->control_blocks);
        if (program->control_buffers) free(program->control_buffers);
        if (program->buffers) free(program->buffers);
        free(program);
    }

private:
    uint32_t buffer_size_;
    uint32_t num_buffers_;
    uint32_t num_channels_;
    uint32_t num_control_buffers_ = 0;
    uint32_t input_buffer_ = 0;
    uint32_t output_buffer_ = 1;
    std::vector<DSPBlock> control_blocks_;
    std::vector<DSPBlock> audio_blocks_;
    std::vector<ModRoute> mod_routes_;
};
