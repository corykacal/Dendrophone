#include "crunchy.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

CrunchyState* crunchy_state_create(float drive,
                                   float bits,
                                   float downsample,
                                   float mix,
                                   float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    CrunchyState* state = static_cast<CrunchyState*>(
        aligned_alloc(64, align_up(sizeof(CrunchyState))));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;
    state->hold_counter = 0;
    state->held_sample = 0.0f;

    state->drive.base = drive;
    state->drive.smoothed = drive;
    state->drive.set_smoothing(10.0f, sample_rate);

    state->bits.base = bits;
    state->bits.smoothed = bits;
    state->bits.set_smoothing(10.0f, sample_rate);

    state->downsample.base = downsample;
    state->downsample.smoothed = downsample;
    state->downsample.set_smoothing(10.0f, sample_rate);

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.set_smoothing(20.0f, sample_rate);

    return state;
}

void crunchy_state_destroy(CrunchyState* state) {
    if (!state) return;
    free(state);
}

// Soft clipping (tanh approximation)
static inline float soft_clip(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void crunchy_op(DSPBlock& b, float* buffers, int n) {
    CrunchyState* s = static_cast<CrunchyState*>(b.state);
    if (!s) {
        float* in = &buffers[b.in_a * n];
        float* out = &buffers[b.out * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    const float mix = std::clamp(s->mix.get_smoothed(), 0.0f, 1.0f);
    const float dry = 1.0f - mix;

    const float drive = std::clamp(s->drive.get_smoothed(), 0.0f, 1.0f);
    const float bits = std::clamp(s->bits.get_smoothed(), 1.0f, 16.0f);
    const float downsample = std::clamp(s->downsample.get_smoothed(), 1.0f, 16.0f);

    const float pre_gain = 1.0f + drive * 7.0f;
    const float post_gain = 1.0f / (1.0f + drive * 2.0f);
    const int downsample_int = std::max(1, static_cast<int>(downsample));
    const float levels = std::pow(2.0f, bits - 1.0f);

    for (int i = 0; i < n; i++) {
        float gained = in[i] * pre_gain;

        // Sample-rate reduction (sample hold)
        if (++s->hold_counter >= downsample_int) {
            s->hold_counter = 0;
            s->held_sample = gained;
        }

        // Bit crush
        float crushed = std::round(s->held_sample * levels) / levels;

        // Soft clip + post-gain normalization
        float wet = soft_clip(crushed) * post_gain;

        out[i] = in[i] * dry + wet * mix;
    }
}
