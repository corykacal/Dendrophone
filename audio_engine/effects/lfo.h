#pragma once

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>
#include <string>

// LFO waveform types
enum class LFOWave {
    Sine,
    Triangle,
    Square,
    Saw,
    Random  // Sample & hold
};

// LFO state
struct LFOState {
    LFOWave wave = LFOWave::Sine;
    float freq_hz = 1.0f;
    float depth = 1.0f;
    float phase = 0.0f;
    float sample_rate = 48000.0f;
    float output = 0.0f;          // Current output value
    float random_value = 0.0f;    // For S&H
    float last_phase = 0.0f;      // For detecting phase wrap
};

// Create LFO state - call from control thread only
LFOState* lfo_state_create(LFOWave wave, float freq_hz, float depth, float sample_rate);

// Destroy LFO state - call from control thread only
void lfo_state_destroy(LFOState* state);

// Control-rate LFO processing
// Computes one output value per block
// frames parameter tells us how much to advance phase
void lfo_op(DSPBlock& b, float* buffers, int frames);

// Parse wave type from string
LFOWave lfo_wave_from_string(const std::string& s);
