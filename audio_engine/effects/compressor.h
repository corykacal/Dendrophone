#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Compressor effect - feed-forward compressor with RMS detection
struct CompressorState {
    float envelope;             // Current envelope level (linear)
    float gain_reduction;       // Current gain reduction (linear)
    float sample_rate;

    // Modulatable parameters
    Param threshold_db;         // Compression threshold in dB (-60.0 - 0.0)
    Param ratio;                // Compression ratio (1.0 - 20.0)
    Param attack_ms;            // Attack time in ms (0.1 - 100.0)
    Param release_ms;           // Release time in ms (10.0 - 1000.0)
    Param makeup_gain_db;       // Makeup gain in dB (0.0 - 40.0)
    Param mix;                  // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize compressor state
CompressorState* compressor_state_create(float threshold_db,
                                           float ratio,
                                           float attack_ms,
                                           float release_ms,
                                           float makeup_gain_db,
                                           float mix,
                                           float sample_rate);

// Control thread: free compressor state
void compressor_state_destroy(CompressorState* state);

// Audio thread: process compressor effect
void compressor_op(DSPBlock& b, float* buffers, int n);

#endif // COMPRESSOR_H
