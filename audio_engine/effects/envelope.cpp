#include "envelope.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

EnvelopeState* envelope_state_create(
    EnvelopeType type,
    float attack_ms,
    float decay_ms,
    float sustain_level,
    float release_ms,
    float sample_rate
) {
    EnvelopeState* state = static_cast<EnvelopeState*>(aligned_alloc(64, sizeof(EnvelopeState)));
    if (!state) return nullptr;

    state->type = type;
    state->stage = EnvelopeStage::Idle;

    // Initialize parameters
    state->attack_ms.base = attack_ms;
    state->attack_ms.mod = 0.0f;
    state->attack_ms.smoothed = attack_ms;
    state->attack_ms.smooth_coeff = 0.0f;

    state->decay_ms.base = decay_ms;
    state->decay_ms.mod = 0.0f;
    state->decay_ms.smoothed = decay_ms;
    state->decay_ms.smooth_coeff = 0.0f;

    state->sustain_level.base = sustain_level;
    state->sustain_level.mod = 0.0f;
    state->sustain_level.smoothed = sustain_level;
    state->sustain_level.smooth_coeff = 0.0f;

    state->release_ms.base = release_ms;
    state->release_ms.mod = 0.0f;
    state->release_ms.smoothed = release_ms;
    state->release_ms.smooth_coeff = 0.0f;

    state->gate.base = 0.0f;
    state->gate.mod = 0.0f;
    state->gate.smoothed = 0.0f;
    state->gate.smooth_coeff = 0.0f;

    state->trigger.base = 0.0f;
    state->trigger.mod = 0.0f;
    state->trigger.smoothed = 0.0f;
    state->trigger.smooth_coeff = 0.0f;

    state->current_value = 0.0f;
    state->output = 0.0f;
    state->last_gate = 0.0f;
    state->last_trigger = 0.0f;
    state->release_start_value = 0.0f;
    state->sample_rate = sample_rate;

    state->attack_coeff = 0.0f;
    state->decay_coeff = 0.0f;
    state->release_coeff = 0.0f;

    return state;
}

void envelope_state_destroy(EnvelopeState* state) {
    if (state) free(state);
}

EnvelopeType envelope_type_from_string(const std::string& s) {
    if (s == "adsr" || s == "ADSR") return EnvelopeType::ADSR;
    if (s == "ad" || s == "AD") return EnvelopeType::AD;
    if (s == "ar" || s == "AR") return EnvelopeType::AR;
    if (s == "ahr" || s == "AHR") return EnvelopeType::AHR;
    return EnvelopeType::ADSR;  // Default
}

// Calculate exponential coefficient for time constant
// time_ms: time constant in milliseconds
// frames: number of frames per block
// sample_rate: sample rate in Hz
// Returns coefficient for: value += (target - value) * coeff
static float calculate_coeff(float time_ms, int frames, float sample_rate) {
    if (time_ms <= 0.0f) return 1.0f;  // Instant

    // Time constant in samples
    float time_samples = (time_ms / 1000.0f) * sample_rate;

    // For control-rate processing, we need to scale by frames per block
    // Exponential approach: 1 - exp(-frames / time_samples)
    float coeff = 1.0f - expf(-frames / time_samples);

    return std::min(1.0f, std::max(0.0f, coeff));
}

void envelope_op(DSPBlock& b, float* buffers, int frames) {
    EnvelopeState* s = static_cast<EnvelopeState*>(b.state);
    if (!s) return;

    // Get current parameter values (base + modulation)
    float attack_ms = s->attack_ms.get();
    float decay_ms = s->decay_ms.get();
    float sustain_level = std::min(1.0f, std::max(0.0f, s->sustain_level.get()));
    float release_ms = s->release_ms.get();
    float gate_value = s->gate.get();
    float trigger_value = s->trigger.get();

    // Calculate coefficients for this block
    s->attack_coeff = calculate_coeff(attack_ms, frames, s->sample_rate);
    s->decay_coeff = calculate_coeff(decay_ms, frames, s->sample_rate);
    s->release_coeff = calculate_coeff(release_ms, frames, s->sample_rate);

    // Detect gate on (0.0 → 1.0 transition)
    bool gate_on = (gate_value > 0.5f && s->last_gate <= 0.5f);
    bool gate_off = (gate_value <= 0.5f && s->last_gate > 0.5f);
    s->last_gate = gate_value;

    // Detect trigger (0.0 → 1.0 transition)
    bool triggered = (trigger_value > 0.5f && s->last_trigger <= 0.5f);
    s->last_trigger = trigger_value;

    // State machine: handle triggers and gates
    if (gate_on || triggered) {
        // Start attack phase
        s->stage = EnvelopeStage::Attack;
    } else if (gate_off && s->stage != EnvelopeStage::Idle && s->stage != EnvelopeStage::Release) {
        // Gate released - enter release phase
        s->stage = EnvelopeStage::Release;
        s->release_start_value = s->current_value;
    }

    // Process envelope stage
    switch (s->stage) {
        case EnvelopeStage::Idle:
            // Envelope is idle - output 0
            s->current_value = 0.0f;
            break;

        case EnvelopeStage::Attack:
            // Attack: ramp up from current value to 1.0
            s->current_value += (1.0f - s->current_value) * s->attack_coeff;

            // Check if attack is complete (reached ~99% of target)
            if (s->current_value >= 0.99f) {
                s->current_value = 1.0f;

                // Transition to next stage based on envelope type
                if (s->type == EnvelopeType::ADSR) {
                    s->stage = EnvelopeStage::Decay;
                } else if (s->type == EnvelopeType::AD) {
                    s->stage = EnvelopeStage::Decay;
                } else if (s->type == EnvelopeType::AR) {
                    s->stage = EnvelopeStage::Sustain;
                } else if (s->type == EnvelopeType::AHR) {
                    s->stage = EnvelopeStage::Sustain;  // Hold phase (same as sustain)
                }
            }
            break;

        case EnvelopeStage::Decay:
            // Decay: ramp down from 1.0 to sustain level
            s->current_value += (sustain_level - s->current_value) * s->decay_coeff;

            // Check if decay is complete
            if (s->type == EnvelopeType::AD) {
                // AD envelope: decay to 0 and go idle
                if (s->current_value <= 0.01f) {
                    s->current_value = 0.0f;
                    s->stage = EnvelopeStage::Idle;
                }
            } else {
                // ADSR: decay to sustain level
                if (fabsf(s->current_value - sustain_level) < 0.01f) {
                    s->current_value = sustain_level;
                    s->stage = EnvelopeStage::Sustain;
                }
            }
            break;

        case EnvelopeStage::Sustain:
            // Sustain: hold at sustain level (or 1.0 for AR type)
            if (s->type == EnvelopeType::ADSR) {
                s->current_value = sustain_level;
            } else if (s->type == EnvelopeType::AR || s->type == EnvelopeType::AHR) {
                s->current_value = 1.0f;
            }

            // For AD type, we never reach sustain (handled in decay)
            // For AR/AHR, sustain until gate off
            // Gate off is handled above to transition to release
            break;

        case EnvelopeStage::Release:
            // Release: ramp down from current value to 0
            s->current_value += (0.0f - s->current_value) * s->release_coeff;

            // Check if release is complete
            if (s->current_value <= 0.01f) {
                s->current_value = 0.0f;
                s->stage = EnvelopeStage::Idle;
            }
            break;
    }

    // Store output
    s->output = s->current_value;

    // Write to output buffer (control buffer at b.out)
    buffers[b.out] = s->output;
}
