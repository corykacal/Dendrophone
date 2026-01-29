#pragma once

#include "../core/dsp_block.h"
#include "../core/dsp_param.h"
#include <cstdint>
#include <string>

// Envelope types
enum class EnvelopeType {
    ADSR,   // Full 4-stage: Attack-Decay-Sustain-Release
    AD,     // Attack-Decay only (one-shot, no sustain)
    AR,     // Attack-Release (no decay, no sustain level)
    AHR     // Attack-Hold-Release (fixed hold time)
};

// Envelope stages
enum class EnvelopeStage {
    Idle,
    Attack,
    Decay,
    Sustain,
    Release
};

// Envelope state
struct EnvelopeState {
    EnvelopeType type = EnvelopeType::ADSR;
    EnvelopeStage stage = EnvelopeStage::Idle;

    // ADSR parameters (modulatable)
    Param attack_ms;
    Param decay_ms;
    Param sustain_level;
    Param release_ms;

    // Trigger/gate inputs (modulatable)
    Param gate;         // 0.0 = off, 1.0 = on (gate signal)
    Param trigger;      // 0.0 → 1.0 edge retriggers envelope

    // Internal state
    float current_value = 0.0f;      // Current envelope output (0-1)
    float output = 0.0f;             // Scaled output value
    float last_gate = 0.0f;          // For gate edge detection
    float last_trigger = 0.0f;       // For trigger edge detection
    float release_start_value = 0.0f; // Value when release started
    float sample_rate = 48000.0f;

    // Smoothing coefficients (calculated from time parameters)
    float attack_coeff = 0.0f;
    float decay_coeff = 0.0f;
    float release_coeff = 0.0f;
};

// Create envelope state - call from control thread only
EnvelopeState* envelope_state_create(
    EnvelopeType type,
    float attack_ms,
    float decay_ms,
    float sustain_level,
    float release_ms,
    float sample_rate
);

// Destroy envelope state - call from control thread only
void envelope_state_destroy(EnvelopeState* state);

// Control-rate envelope processing
// Computes one output value per block
// frames parameter tells us how much time has passed
void envelope_op(DSPBlock& b, float* buffers, int frames);

// Parse envelope type from string
EnvelopeType envelope_type_from_string(const std::string& s);
