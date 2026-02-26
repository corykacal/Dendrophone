#ifndef GLITCH_H
#define GLITCH_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Glitch/stutter effect with buffer freeze and rhythmic retrigger
struct GlitchState {
    float* buffer;              // Freeze buffer
    uint32_t buffer_size;       // Buffer size in samples
    uint32_t capture_length;    // Length of captured audio
    uint32_t write_pos;         // Write position (when not frozen)
    float read_pos;             // Read position (float for speed control)

    bool frozen;                // Is buffer currently frozen?
    uint32_t stutter_counter;   // Counter for stutter rhythm
    uint32_t random_seed;       // Random seed for variations

    float xfade_gain;           // Crossfade envelope amplitude (0.0–1.0)
    int32_t xfade_dir;          // -1 = fading out, 0 = idle, +1 = fading in

    float sample_rate;

    // Modulatable parameters
    Param freeze;               // Freeze on/off (0.0 = live, 1.0 = frozen)
    Param stutter_rate_hz;      // Stutter retrigger rate in Hz (0.5 - 50.0)
    Param capture_size_ms;      // Size of freeze buffer in milliseconds (10.0 - 1000.0)
    Param speed;                // Playback speed (0.25 - 4.0)
    Param randomize;            // Randomization amount (0.0 - 1.0)
    Param mix;                  // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize glitch state
GlitchState* glitch_state_create(float freeze,
                                  float stutter_rate_hz,
                                  float capture_size_ms,
                                  float speed,
                                  float randomize,
                                  float mix,
                                  float sample_rate);

// Control thread: free glitch state
void glitch_state_destroy(GlitchState* state);

// Audio thread: process glitch/stutter
void glitch_op(DSPBlock& b, float* buffers, int n);

#endif // GLITCH_H
