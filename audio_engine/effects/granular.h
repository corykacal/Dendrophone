#ifndef GRANULAR_H
#define GRANULAR_H

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>
#include <string>

// Granular synthesis engine with multiple simultaneous grains
struct GranularState {
    static constexpr int MAX_GRAINS = 16;  // Maximum simultaneous grains
    static constexpr int WINDOW_TABLE_SIZE = 1024;

    // Window function types
    enum class WindowType {
        Hann,
        Blackman,
        Triangle,
        Tukey
    };

    // Per-grain state
    struct Grain {
        bool active;                // Is this grain currently playing?
        float phase;                // Current position in grain (0.0 - 1.0)
        float phase_inc;            // Phase increment per sample
        float buffer_pos;           // Position in source buffer (float for interpolation)
        float pitch_ratio;          // Playback speed for this grain
        float amplitude;            // Grain amplitude
        uint32_t length_samples;    // Total grain length in samples
    };

    float* buffer;                  // Source audio buffer (circular)
    uint32_t buffer_size;           // Buffer size in samples
    uint32_t write_pos;             // Write position in buffer

    Grain grains[MAX_GRAINS];       // Array of grain voices
    int num_active_grains;          // Current number of active grains

    // Window function lookup table
    float* window_table;
    WindowType window_type;

    // Grain scheduling
    float grain_spawn_counter;      // Counter for scheduling next grain
    uint32_t random_seed;           // For randomization

    float sample_rate;

    // Modulatable parameters
    Param grain_size_ms;            // Grain length in milliseconds (5.0 - 200.0)
    Param density;                  // Grains per second (1.0 - 50.0)
    Param pitch_shift;              // Pitch shift in semitones (-12.0 - 12.0)
    Param pitch_random;             // Random pitch variation in semitones (0.0 - 12.0)
    Param position_random;          // Random position jitter in milliseconds (0.0 - 500.0)
    Param mix;                      // Dry/wet mix (0.0 - 1.0)
} __attribute__((aligned(64)));

// Control thread: allocate and initialize granular state
GranularState* granular_state_create(float grain_size_ms,
                                      float density,
                                      float pitch_shift,
                                      float pitch_random,
                                      float position_random,
                                      GranularState::WindowType window_type,
                                      float mix,
                                      float sample_rate);

// Control thread: free granular state
void granular_state_destroy(GranularState* state);

// Audio thread: process granular synthesis
void granular_op(DSPBlock& b, float* buffers, int n);

// Helper function to convert string to window type
GranularState::WindowType granular_window_from_string(const std::string& window_str);

#endif // GRANULAR_H
