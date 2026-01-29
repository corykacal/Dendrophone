#include "svfilter.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>

// Convert string to filter mode
SVFilterState::Mode svfilter_mode_from_string(const std::string& mode_str) {
    if (mode_str == "lowpass" || mode_str == "lp") {
        return SVFilterState::Mode::LowPass;
    } else if (mode_str == "highpass" || mode_str == "hp") {
        return SVFilterState::Mode::HighPass;
    } else if (mode_str == "bandpass" || mode_str == "bp") {
        return SVFilterState::Mode::BandPass;
    } else if (mode_str == "notch") {
        return SVFilterState::Mode::Notch;
    }
    return SVFilterState::Mode::LowPass;  // Default
}

SVFilterState* svfilter_state_create(SVFilterState::Mode mode,
                                      float cutoff,
                                      float resonance,
                                      float mix,
                                      float sample_rate) {
    SVFilterState* state = static_cast<SVFilterState*>(
        aligned_alloc(64, sizeof(SVFilterState)));
    if (!state) return nullptr;

    // Initialize filter mode
    state->mode = mode;

    // Initialize SVF state variables
    state->low = 0.0f;
    state->band = 0.0f;

    state->sample_rate = sample_rate;

    // Initialize cutoff parameter (with smoothing to prevent zipper noise)
    state->cutoff.base = cutoff;
    state->cutoff.mod = 0.0f;
    state->cutoff.smoothed = cutoff;
    state->cutoff.set_smoothing(5.0f, sample_rate);  // 5ms smoothing

    // Initialize resonance parameter (slower smoothing to prevent oscillation clicks)
    state->resonance.base = resonance;
    state->resonance.mod = 0.0f;
    state->resonance.smoothed = resonance;
    state->resonance.set_smoothing(10.0f, sample_rate);  // 10ms smoothing

    // Initialize mix parameter (no smoothing needed)
    state->mix.base = mix;
    state->mix.mod = 0.0f;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

void svfilter_state_destroy(SVFilterState* state) {
    if (!state) return;
    free(state);
}

void svfilter_op(DSPBlock& b, float* buffers, int n) {
    SVFilterState* s = static_cast<SVFilterState*>(b.state);
    if (!s) {
        // Passthrough fallback if no state
        float* in = &buffers[b.in_a * n];
        float* out = &buffers[b.out * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    const float mix = std::clamp(s->mix.get(), 0.0f, 1.0f);
    const float dry = 1.0f - mix;

    for (int i = 0; i < n; i++) {
        // Get smoothed parameters
        float cutoff_hz = s->cutoff.get_smoothed();
        // Clamp cutoff to prevent instability (10 Hz min, Nyquist/2 max)
        cutoff_hz = std::clamp(cutoff_hz, 10.0f, s->sample_rate * 0.45f);

        float q_val = s->resonance.get_smoothed();
        // Clamp resonance to safe range (0.1 min prevents divide-by-zero, 12.0 max)
        q_val = std::clamp(q_val, 0.1f, 12.0f);

        // Compute SVF coefficients
        // f = 2 * sin(π * cutoff / sample_rate) - frequency coefficient
        float f = 2.0f * sinf(M_PI * cutoff_hz / s->sample_rate);
        // q = 1.0 / resonance - damping coefficient
        float q = 1.0f / q_val;

        // State Variable Filter topology:
        // low = integrator for lowpass
        // band = integrator for bandpass
        // high = input - low - q * band (highpass is derived)
        s->low += f * s->band;
        float high = in[i] - s->low - q * s->band;
        s->band += f * high;
        float notch = high + s->low;

        // Select output based on mode
        float filtered;
        switch (s->mode) {
            case SVFilterState::Mode::LowPass:
                filtered = s->low;
                break;
            case SVFilterState::Mode::HighPass:
                filtered = high;
                break;
            case SVFilterState::Mode::BandPass:
                filtered = s->band;
                break;
            case SVFilterState::Mode::Notch:
                filtered = notch;
                break;
            default:
                filtered = s->low;
                break;
        }

        // Mix dry/wet
        out[i] = in[i] * dry + filtered * mix;
    }
}
