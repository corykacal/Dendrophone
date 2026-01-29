#pragma once

#include "dsp_block.h"
#include "dsp_param.h"
#include <cstdint>
#include <cstring>

// Modulation routing: connects a control output to a parameter
struct ModRoute {
    uint32_t control_buffer;   // Index of control buffer containing modulation value
    Param* target;             // Pointer to parameter to modulate
    float scale;               // Modulation depth multiplier
};

struct DSPProgram {
    // Audio buffers (one per channel/internal signal)
    float* buffers;
    uint32_t num_buffers;
    uint32_t buffer_size;      // Samples per buffer (frames_per_buffer)

    // Control buffers (one float per control signal)
    float* control_buffers;
    uint32_t num_control_buffers;

    // Control-rate blocks (LFOs, envelopes - run once per buffer)
    DSPBlock* control_blocks;
    uint32_t num_control_blocks;

    // Audio-rate blocks (effects - run per sample)
    DSPBlock* audio_blocks;
    uint32_t num_audio_blocks;

    // Modulation routing
    ModRoute* mod_routes;
    uint32_t num_mod_routes;

    // I/O configuration
    uint32_t input_buffer;     // First input buffer index
    uint32_t output_buffer;    // First output buffer index
    uint32_t num_channels;     // Number of audio channels

    // Process interleaved audio
    // in/out: interleaved audio [L0,R0,L1,R1,...] with frames*channels samples
    // frames: number of audio frames (NOT samples)
    inline void process(float* in, float* out, int frames) {
        // 1. Deinterleave input into separate channel buffers
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            float* dst = &buffers[(input_buffer + ch) * buffer_size];
            for (int i = 0; i < frames; i++) {
                dst[i] = in[i * num_channels + ch];
            }
        }

        // 2. Reset all modulation accumulators
        for (uint32_t i = 0; i < num_mod_routes; i++) {
            if (mod_routes[i].target) {
                mod_routes[i].target->reset_mod();
            }
        }

        // 3. Run control-rate blocks (once per buffer)
        for (uint32_t i = 0; i < num_control_blocks; i++) {
            control_blocks[i].fn(control_blocks[i], control_buffers, frames);
        }

        // 4. Apply modulation routing
        for (uint32_t i = 0; i < num_mod_routes; i++) {
            ModRoute& route = mod_routes[i];
            if (route.target) {
                float mod_value = control_buffers[route.control_buffer] * route.scale;
                route.target->add_mod(mod_value);
            }
        }

        // 5. Run audio-rate blocks
        for (uint32_t i = 0; i < num_audio_blocks; i++) {
            audio_blocks[i].fn(audio_blocks[i], buffers, frames);
        }

        // 6. Interleave output from separate channel buffers
        for (uint32_t ch = 0; ch < num_channels; ch++) {
            float* src = &buffers[(output_buffer + ch) * buffer_size];
            for (int i = 0; i < frames; i++) {
                out[i * num_channels + ch] = src[i];
            }
        }
    }
};
