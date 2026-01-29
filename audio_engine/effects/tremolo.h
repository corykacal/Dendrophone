#ifndef TREMOLO_H
#define TREMOLO_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>
#include <string>

// Tremolo effect - LFO-modulated amplitude
struct TremoloState {
    enum class Waveform { Sine, Triangle, Square };

    float lfo_phase;        // LFO phase (0 to 2π)
    float sample_rate;
    Waveform waveform;

    // Modulatable parameters
    Param rate_hz;          // LFO rate in Hz (0.1 - 20.0)
    Param depth;            // Modulation depth (0.0 - 1.0)
    Param mix;              // Dry/wet mix (0.0 - 1.0)

    // Non-modulatable
    float stereo_phase;     // L/R phase offset (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize tremolo state
TremoloState* tremolo_state_create(float rate_hz,
                                     float depth,
                                     const std::string& waveform,
                                     float stereo_phase,
                                     float mix,
                                     float sample_rate);

// Control thread: free tremolo state
void tremolo_state_destroy(TremoloState* state);

// Audio thread: process tremolo effect
void tremolo_op(DSPBlock& b, float* buffers, int n);

#endif // TREMOLO_H
