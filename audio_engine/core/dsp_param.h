#pragma once

#include <cstdint>
#include <cmath>

// A modulatable parameter
// base: the static value from .dpt params
// mod: accumulated modulation from control signals (reset each block)
// smoothed: the actual value used by DSP (optionally smoothed)
struct Param {
    float base = 0.0f;
    float mod = 0.0f;
    float smoothed = 0.0f;
    float smooth_coeff = 0.0f;  // 0 = no smoothing

    // Get the current value (base + modulation)
    inline float get() const {
        return base + mod;
    }

    // Get smoothed value and update smoother
    inline float get_smoothed() {
        float target = base + mod;
        if (smooth_coeff > 0.0f) {
            smoothed += (target - smoothed) * smooth_coeff;
            return smoothed;
        }
        return target;
    }

    // Reset modulation accumulator (call at start of each block)
    inline void reset_mod() {
        mod = 0.0f;
    }

    // Add modulation
    inline void add_mod(float value) {
        mod += value;
    }

    // Initialize smoothing coefficient from time constant
    // tau_ms: time to reach ~63% of target
    // sample_rate: audio sample rate
    void set_smoothing(float tau_ms, float sample_rate) {
        if (tau_ms <= 0.0f) {
            smooth_coeff = 0.0f;
        } else {
            smooth_coeff = 1.0f - std::exp(-1000.0f / (tau_ms * sample_rate));
        }
        smoothed = base;
    }
};

// Control output buffer - stores one value per block
struct ControlBuffer {
    float value = 0.0f;
};

// Pointer to a parameter to modulate
struct ParamTarget {
    Param* param = nullptr;
    float scale = 1.0f;  // Modulation depth scaling
};
