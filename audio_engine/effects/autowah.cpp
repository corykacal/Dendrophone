#include "autowah.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AutoWahState* autowah_state_create(float sensitivity,
                                     float attack_ms,
                                     float release_ms,
                                     float frequency_min_hz,
                                     float frequency_max_hz,
                                     float resonance,
                                     float mix,
                                     float sample_rate) {
    AutoWahState* state = static_cast<AutoWahState*>(
        aligned_alloc(64, sizeof(AutoWahState)));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;
    state->envelope = 0.0f;
    state->filter_low = 0.0f;
    state->filter_band = 0.0f;

    // Initialize parameters
    state->sensitivity.base = sensitivity;
    state->sensitivity.smoothed = sensitivity;
    state->sensitivity.smooth_coeff = 0.0f;

    state->attack_ms.base = attack_ms;
    state->attack_ms.smoothed = attack_ms;
    state->attack_ms.set_smoothing(10.0f, sample_rate);

    state->release_ms.base = release_ms;
    state->release_ms.smoothed = release_ms;
    state->release_ms.set_smoothing(10.0f, sample_rate);

    state->frequency_min_hz.base = frequency_min_hz;
    state->frequency_min_hz.smoothed = frequency_min_hz;
    state->frequency_min_hz.set_smoothing(10.0f, sample_rate);

    state->frequency_max_hz.base = frequency_max_hz;
    state->frequency_max_hz.smoothed = frequency_max_hz;
    state->frequency_max_hz.set_smoothing(10.0f, sample_rate);

    state->resonance.base = resonance;
    state->resonance.smoothed = resonance;
    state->resonance.set_smoothing(10.0f, sample_rate);

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

void autowah_state_destroy(AutoWahState* state) {
    if (!state) return;
    free(state);
}

void autowah_op(DSPBlock& b, float* buffers, int n) {
    AutoWahState* s = static_cast<AutoWahState*>(b.state);
    if (!s) {
        float* in = &buffers[b.in_a * n];
        float* out = &buffers[b.out * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    const float mix = std::clamp(s->mix.get(), 0.0f, 1.0f);
    const float dry = 1.0f - mix;

    // Get parameters
    float sensitivity = std::clamp(s->sensitivity.get(), 0.0f, 1.0f);
    float attack_ms = std::clamp(s->attack_ms.get_smoothed(), 1.0f, 100.0f);
    float release_ms = std::clamp(s->release_ms.get_smoothed(), 10.0f, 1000.0f);
    float freq_min = std::clamp(s->frequency_min_hz.get_smoothed(), 100.0f, 2000.0f);
    float freq_max = std::clamp(s->frequency_max_hz.get_smoothed(), 500.0f, 8000.0f);
    float resonance = std::clamp(s->resonance.get_smoothed(), 0.5f, 10.0f);

    // Ensure min < max
    if (freq_min > freq_max) {
        float temp = freq_min;
        freq_min = freq_max;
        freq_max = temp;
    }

    // Calculate attack and release coefficients
    float attack_coeff = expf(-1.0f / (attack_ms * s->sample_rate / 1000.0f));
    float release_coeff = expf(-1.0f / (release_ms * s->sample_rate / 1000.0f));

    for (int i = 0; i < n; i++) {
        // Envelope follower (peak detection with attack/release)
        float input_level = fabsf(in[i]) * sensitivity;

        if (input_level > s->envelope) {
            // Attack
            s->envelope = attack_coeff * s->envelope + (1.0f - attack_coeff) * input_level;
        } else {
            // Release
            s->envelope = release_coeff * s->envelope + (1.0f - release_coeff) * input_level;
        }

        // Map envelope to filter frequency (linear mapping)
        float cutoff_hz = freq_min + s->envelope * (freq_max - freq_min);
        cutoff_hz = std::clamp(cutoff_hz, 20.0f, s->sample_rate * 0.48f);

        // Calculate SVF coefficients
        float f = 2.0f * sinf(M_PI * cutoff_hz / s->sample_rate);
        float q = 1.0f / resonance;

        // State variable filter (bandpass mode)
        s->filter_low += f * s->filter_band;
        float hp = in[i] - s->filter_low - q * s->filter_band;
        s->filter_band += f * hp;

        // Output bandpass (use filter_band directly)
        float wet = s->filter_band;

        // Mix dry and wet
        out[i] = in[i] * dry + wet * mix;
    }
}
