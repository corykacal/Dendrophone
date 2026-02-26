#ifndef CHORUS_H
#define CHORUS_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>
#include <string>

// Chorus effect with multiple modulated delay lines
struct ChorusState {
    static constexpr int MAX_VOICES = 4;

    float* buffer;              // Shared delay buffer
    uint32_t buffer_size;       // Buffer size in samples
    uint32_t write_pos;         // Write position in buffer

    float lfo_cos[MAX_VOICES];   // Magic-circle cosine state per voice
    float lfo_sin[MAX_VOICES];   // Magic-circle sine state per voice
    float lfo_epsilon;           // 2*sin(π*rate/sr), recomputed when rate changes
    float last_rate;             // Cached rate for epsilon recompute guard
    float sample_rate;

    int num_voices;             // Number of chorus voices (2-4)

    // Modulatable parameters
    Param rate_hz;              // LFO rate in Hz (0.1 - 10.0)
    Param depth;                // Modulation depth (0.0 - 1.0)
    Param mix;                  // Dry/wet mix (0.0 - 1.0)

    // Non-modulatable parameters
    float stereo_width;         // Stereo width (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize chorus state
ChorusState* chorus_state_create(float rate_hz,
                                   float depth,
                                   int voices,
                                   float stereo_width,
                                   float mix,
                                   float sample_rate);

// Control thread: free chorus state
void chorus_state_destroy(ChorusState* state);

// Audio thread: process chorus effect
void chorus_op(DSPBlock& b, float* buffers, int n);

#endif // CHORUS_H
