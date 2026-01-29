#ifndef MULTITAP_DELAY_H
#define MULTITAP_DELAY_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Multi-tap delay with 8 independent delay taps
struct MultiTapDelayState {
    static constexpr int MAX_TAPS = 8;

    float* buffer;                      // Shared ring buffer
    uint32_t buffer_size;               // Buffer size in samples
    uint32_t max_delay_samples;         // Maximum delay capacity
    uint32_t write_pos;                 // Write position

    // Per-tap configuration
    struct TapConfig {
        Param time_ms;                  // Tap delay time in milliseconds
        Param gain;                     // Tap output gain (0.0 - 2.0)
        bool enabled;                   // Whether this tap is active
    };

    TapConfig taps[MAX_TAPS];
    int num_active_taps;                // Number of taps currently in use

    float sample_rate;

    // Global modulatable parameters
    Param feedback;                     // Global feedback (0.0 - 0.99)
    Param mix;                          // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize multi-tap delay state
MultiTapDelayState* multitap_delay_state_create(int num_taps,
                                                  const float* tap_times_ms,
                                                  const float* tap_gains,
                                                  float feedback,
                                                  float mix,
                                                  float sample_rate);

// Control thread: free multi-tap delay state
void multitap_delay_state_destroy(MultiTapDelayState* state);

// Audio thread: process multi-tap delay
void multitap_delay_op(DSPBlock& b, float* buffers, int n);

#endif // MULTITAP_DELAY_H
