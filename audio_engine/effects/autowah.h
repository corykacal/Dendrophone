#ifndef AUTOWAH_H
#define AUTOWAH_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Auto-wah effect - envelope follower controlling bandpass filter
struct AutoWahState {
    float envelope;         // Current envelope level
    float filter_low;       // Bandpass filter state (lowpass section)
    float filter_band;      // Bandpass filter state (bandpass section)
    float sample_rate;

    // Modulatable parameters
    Param sensitivity;      // Envelope sensitivity (0.0 - 1.0)
    Param attack_ms;        // Envelope attack time (1.0 - 100.0 ms)
    Param release_ms;       // Envelope release time (10.0 - 1000.0 ms)
    Param frequency_min_hz; // Minimum filter frequency (100.0 - 2000.0 Hz)
    Param frequency_max_hz; // Maximum filter frequency (500.0 - 8000.0 Hz)
    Param resonance;        // Filter resonance/Q (0.5 - 10.0)
    Param mix;              // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize autowah state
AutoWahState* autowah_state_create(float sensitivity,
                                     float attack_ms,
                                     float release_ms,
                                     float frequency_min_hz,
                                     float frequency_max_hz,
                                     float resonance,
                                     float mix,
                                     float sample_rate);

// Control thread: free autowah state
void autowah_state_destroy(AutoWahState* state);

// Audio thread: process autowah effect
void autowah_op(DSPBlock& b, float* buffers, int n);

#endif // AUTOWAH_H
