#include "overdrive.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

OverdriveState* overdrive_state_create(float drive,
                                         float tone,
                                         float level,
                                         const std::string& type,
                                         float mix,
                                         float sample_rate) {
    OverdriveState* state = static_cast<OverdriveState*>(
        aligned_alloc(64, sizeof(OverdriveState)));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;
    state->tone_filter = 0.0f;

    // Parse type
    if (type == "hard") {
        state->type = OverdriveState::Type::Hard;
    } else if (type == "asymmetric") {
        state->type = OverdriveState::Type::Asymmetric;
    } else {
        state->type = OverdriveState::Type::Soft;
    }

    // Initialize parameters
    state->drive.base = drive;
    state->drive.smoothed = drive;
    state->drive.set_smoothing(10.0f, sample_rate);

    state->tone.base = tone;
    state->tone.smoothed = tone;
    state->tone.smooth_coeff = 0.0f;

    state->level.base = level;
    state->level.smoothed = level;
    state->level.set_smoothing(10.0f, sample_rate);

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

void overdrive_state_destroy(OverdriveState* state) {
    if (!state) return;
    free(state);
}

// Soft clipping (tanh approximation)
static inline float soft_clip(float x) {
    // Fast tanh approximation
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Hard clipping
static inline float hard_clip(float x) {
    return std::clamp(x, -1.0f, 1.0f);
}

// Asymmetric clipping
static inline float asymmetric_clip(float x) {
    if (x > 0.0f) {
        return soft_clip(x * 0.7f) / 0.7f;
    } else {
        return hard_clip(x * 1.3f) / 1.3f;
    }
}

void overdrive_op(DSPBlock& b, float* buffers, int n) {
    OverdriveState* s = static_cast<OverdriveState*>(b.state);
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
    float drive = std::clamp(s->drive.get_smoothed(), 0.0f, 1.0f);
    float tone = std::clamp(s->tone.get(), 0.0f, 1.0f);
    float level = std::clamp(s->level.get_smoothed(), 0.0f, 2.0f);

    // Calculate pre-gain (drive maps to 1x - 50x)
    float pre_gain = 1.0f + drive * 49.0f;

    // Calculate tone filter coefficient (maps tone to cutoff frequency)
    float cutoff_hz = 500.0f + tone * 4500.0f;  // 500Hz to 5kHz
    float omega = 2.0f * 3.14159265f * cutoff_hz / s->sample_rate;
    float tone_coeff = std::min(omega, 1.0f);

    for (int i = 0; i < n; i++) {
        // Apply pre-gain
        float gained = in[i] * pre_gain;

        // Apply waveshaping
        float clipped;
        switch (s->type) {
            case OverdriveState::Type::Soft:
                clipped = soft_clip(gained);
                break;
            case OverdriveState::Type::Hard:
                clipped = hard_clip(gained);
                break;
            case OverdriveState::Type::Asymmetric:
                clipped = asymmetric_clip(gained);
                break;
            default:
                clipped = gained;
        }

        // Apply tone filter (simple 1-pole lowpass)
        s->tone_filter += tone_coeff * (clipped - s->tone_filter);

        // Apply output level
        float wet = s->tone_filter * level;

        // Mix dry and wet
        out[i] = in[i] * dry + wet * mix;
    }
}
