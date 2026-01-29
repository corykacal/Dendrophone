#include "tremolo.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TremoloState* tremolo_state_create(float rate_hz,
                                     float depth,
                                     const std::string& waveform,
                                     float stereo_phase,
                                     float mix,
                                     float sample_rate) {
    TremoloState* state = static_cast<TremoloState*>(
        aligned_alloc(64, sizeof(TremoloState)));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;
    state->lfo_phase = 0.0f;

    // Parse waveform
    if (waveform == "triangle") {
        state->waveform = TremoloState::Waveform::Triangle;
    } else if (waveform == "square") {
        state->waveform = TremoloState::Waveform::Square;
    } else {
        state->waveform = TremoloState::Waveform::Sine;
    }

    state->stereo_phase = std::clamp(stereo_phase, 0.0f, 1.0f);

    // Initialize parameters
    state->rate_hz.base = rate_hz;
    state->rate_hz.smoothed = rate_hz;
    state->rate_hz.set_smoothing(20.0f, sample_rate);

    state->depth.base = depth;
    state->depth.smoothed = depth;
    state->depth.smooth_coeff = 0.0f;

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

void tremolo_state_destroy(TremoloState* state) {
    if (!state) return;
    free(state);
}

static inline float generate_lfo(float phase, TremoloState::Waveform waveform) {
    switch (waveform) {
        case TremoloState::Waveform::Sine:
            return sinf(phase);

        case TremoloState::Waveform::Triangle: {
            float t = phase / (2.0f * M_PI);
            if (t < 0.25f) return 4.0f * t;
            else if (t < 0.75f) return 2.0f - 4.0f * t;
            else return 4.0f * t - 4.0f;
        }

        case TremoloState::Waveform::Square:
            return (phase < M_PI) ? 1.0f : -1.0f;

        default:
            return 0.0f;
    }
}

void tremolo_op(DSPBlock& b, float* buffers, int n) {
    TremoloState* s = static_cast<TremoloState*>(b.state);
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
    float rate = std::clamp(s->rate_hz.get_smoothed(), 0.1f, 20.0f);
    float depth = std::clamp(s->depth.get(), 0.0f, 1.0f);

    const float lfo_increment = 2.0f * M_PI * rate / s->sample_rate;

    for (int i = 0; i < n; i++) {
        // Generate LFO value (-1 to 1)
        float lfo = generate_lfo(s->lfo_phase, s->waveform);

        // Convert LFO to gain modulation
        // depth=0: gain=1.0 (no modulation)
        // depth=1: gain oscillates from 0.0 to 2.0, centered at 1.0
        float gain = 1.0f + lfo * depth;

        // Apply tremolo
        float wet = in[i] * gain;
        out[i] = in[i] * dry + wet * mix;

        // Advance LFO phase
        s->lfo_phase += lfo_increment;
        if (s->lfo_phase >= 2.0f * M_PI) {
            s->lfo_phase -= 2.0f * M_PI;
        }
    }
}
