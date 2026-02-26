#ifndef CRUNCHY_H
#define CRUNCHY_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <string>

// Crunchy effect - bit crusher + sample-rate reduction with drive
struct CrunchyState {
    int hold_counter;       // Sample-hold counter for downsampling
    float held_sample;      // Last held sample value
    float sample_rate;

    // Modulatable parameters
    Param drive;            // Pre-gain drive (0.0 - 1.0 → 1× - 8×)
    Param bits;             // Bit depth reduction (1.0 - 16.0)
    Param downsample;       // Sample-hold length (1.0 - 16.0)
    Param mix;              // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize crunchy state
CrunchyState* crunchy_state_create(float drive,
                                   float bits,
                                   float downsample,
                                   float mix,
                                   float sample_rate);

// Control thread: free crunchy state
void crunchy_state_destroy(CrunchyState* state);

// Audio thread: process crunchy effect
void crunchy_op(DSPBlock& b, float* buffers, int n);

#endif // CRUNCHY_H
