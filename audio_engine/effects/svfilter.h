#ifndef SVFILTER_H
#define SVFILTER_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>
#include <string>

// State Variable Filter (SVF) with resonance control
// Provides lowpass, highpass, bandpass, and notch outputs
struct SVFilterState {
    // Filter mode selection
    enum class Mode {
        LowPass,
        HighPass,
        BandPass,
        Notch
    };
    Mode mode;

    // SVF integrator state variables
    float low;   // Lowpass integrator output
    float band;  // Bandpass integrator output

    float sample_rate;

    // Modulatable parameters
    Param cutoff;     // Cutoff frequency in Hz (20.0 - 20000.0)
    Param resonance;  // Resonance/Q factor (0.5 - 10.0, self-oscillates at ~10)
    Param mix;        // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Helper function to convert string to filter mode
SVFilterState::Mode svfilter_mode_from_string(const std::string& mode_str);

// Control thread: allocate and initialize SVF state
SVFilterState* svfilter_state_create(SVFilterState::Mode mode,
                                      float cutoff,
                                      float resonance,
                                      float mix,
                                      float sample_rate);

// Control thread: free SVF state
void svfilter_state_destroy(SVFilterState* state);

// Audio thread: process SVF
void svfilter_op(DSPBlock& b, float* buffers, int n);

#endif // SVFILTER_H
