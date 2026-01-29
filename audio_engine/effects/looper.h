#ifndef LOOPER_H
#define LOOPER_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>

// Phrase looper with record, overdub, and playback modes
struct LooperState {
    // Loop modes
    enum class Mode {
        Bypass,      // Pass through
        Recording,   // Recording into buffer
        Playing,     // Playing loop
        Overdubbing  // Playing + recording (mix with existing)
    };

    float* buffer;              // Loop buffer
    uint32_t buffer_size;       // Total buffer size (60 seconds)
    uint32_t loop_length;       // Actual recorded loop length
    uint32_t write_pos;         // Write position
    uint32_t read_pos;          // Read position

    Mode mode;                  // Current operating mode
    bool loop_recorded;         // Has a loop been recorded?

    float sample_rate;

    // Modulatable parameters
    Param speed;                // Playback speed (0.25 - 4.0, 1.0 = normal)
    Param feedback;             // Overdub feedback (0.0 - 1.0)
    Param mix;                  // Dry/wet mix (0.0 - 1.0)

    // Non-modulatable controls (set via triggers)
    Param record_trigger;       // Trigger recording (0.0 or 1.0)
    Param play_trigger;         // Trigger playback (0.0 or 1.0)
    Param overdub_trigger;      // Trigger overdub (0.0 or 1.0)
    Param clear_trigger;        // Clear loop (0.0 or 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize looper state
LooperState* looper_state_create(float speed, float feedback, float mix, float sample_rate);

// Control thread: free looper state
void looper_state_destroy(LooperState* state);

// Audio thread: process looper
void looper_op(DSPBlock& b, float* buffers, int n);

#endif // LOOPER_H
