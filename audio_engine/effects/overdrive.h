#ifndef OVERDRIVE_H
#define OVERDRIVE_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <string>

// Overdrive effect - soft/hard clipping waveshaper with tone control
struct OverdriveState {
    enum class Type { Soft, Hard, Asymmetric };

    Type type;
    float tone_filter;      // Tone filter state (simple 1-pole LP)
    float sample_rate;

    // Modulatable parameters
    Param drive;            // Drive amount (0.0 - 1.0)
    Param tone;             // Tone control (0.0 - 1.0)
    Param level;            // Output level (0.0 - 2.0)
    Param mix;              // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize overdrive state
OverdriveState* overdrive_state_create(float drive,
                                         float tone,
                                         float level,
                                         const std::string& type,
                                         float mix,
                                         float sample_rate);

// Control thread: free overdrive state
void overdrive_state_destroy(OverdriveState* state);

// Audio thread: process overdrive effect
void overdrive_op(DSPBlock& b, float* buffers, int n);

#endif // OVERDRIVE_H
