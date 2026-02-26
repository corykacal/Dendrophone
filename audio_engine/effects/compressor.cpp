#include "compressor.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

CompressorState* compressor_state_create(float threshold_db,
                                           float ratio,
                                           float attack_ms,
                                           float release_ms,
                                           float makeup_gain_db,
                                           float mix,
                                           float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    CompressorState* state = static_cast<CompressorState*>(
        aligned_alloc(64, align_up(sizeof(CompressorState))));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;
    state->envelope = 0.0f;

    // Initialize parameters
    state->threshold_db.base = threshold_db;
    state->threshold_db.mod = 0.0f;
    state->threshold_db.smoothed = threshold_db;
    state->threshold_db.set_smoothing(10.0f, sample_rate);

    state->ratio.base = ratio;
    state->ratio.mod = 0.0f;
    state->ratio.smoothed = ratio;
    state->ratio.set_smoothing(10.0f, sample_rate);

    state->attack_ms.base = attack_ms;
    state->attack_ms.mod = 0.0f;
    state->attack_ms.smoothed = attack_ms;
    state->attack_ms.set_smoothing(10.0f, sample_rate);

    state->release_ms.base = release_ms;
    state->release_ms.mod = 0.0f;
    state->release_ms.smoothed = release_ms;
    state->release_ms.set_smoothing(10.0f, sample_rate);

    state->makeup_gain_db.base = makeup_gain_db;
    state->makeup_gain_db.mod = 0.0f;
    state->makeup_gain_db.smoothed = makeup_gain_db;
    state->makeup_gain_db.set_smoothing(10.0f, sample_rate);

    state->mix.base = mix;
    state->mix.mod = 0.0f;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

void compressor_state_destroy(CompressorState* state) {
    if (!state) return;
    free(state);
}

// Convert dB to linear
static inline float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

// Convert linear to dB
static inline float linear_to_db(float linear) {
    return 20.0f * log10f(std::max(linear, 1e-10f));
}

void compressor_op(DSPBlock& b, float* buffers, int n) {
    CompressorState* s = static_cast<CompressorState*>(b.state);
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
    float threshold_db = std::clamp(s->threshold_db.get_smoothed(), -60.0f, 0.0f);
    float ratio = std::clamp(s->ratio.get_smoothed(), 1.0f, 20.0f);
    float attack_ms = std::clamp(s->attack_ms.get_smoothed(), 0.1f, 100.0f);
    float release_ms = std::clamp(s->release_ms.get_smoothed(), 10.0f, 1000.0f);
    float makeup_gain_db = std::clamp(s->makeup_gain_db.get_smoothed(), 0.0f, 40.0f);

    // Calculate attack and release coefficients
    float attack_coeff = expf(-1.0f / (attack_ms * s->sample_rate / 1000.0f));
    float release_coeff = expf(-1.0f / (release_ms * s->sample_rate / 1000.0f));

    float threshold_linear = db_to_linear(threshold_db);
    float makeup_gain_linear = db_to_linear(makeup_gain_db);

    for (int i = 0; i < n; i++) {
        // Get input level (absolute value for simple peak detection)
        float input_level = fabsf(in[i]);

        // Update envelope follower with attack/release
        if (input_level > s->envelope) {
            // Attack
            s->envelope = attack_coeff * s->envelope + (1.0f - attack_coeff) * input_level;
        } else {
            // Release
            s->envelope = release_coeff * s->envelope + (1.0f - release_coeff) * input_level;
        }

        // Calculate gain reduction
        float gain_reduction = 1.0f;
        if (s->envelope > threshold_linear) {
            // Convert to dB for calculation
            float envelope_db = linear_to_db(s->envelope);
            float over_threshold_db = envelope_db - threshold_db;

            // Apply compression ratio
            float compressed_db = threshold_db + (over_threshold_db / ratio);

            // Calculate gain reduction in dB
            float gain_reduction_db = compressed_db - envelope_db;

            // Convert back to linear
            gain_reduction = db_to_linear(gain_reduction_db);
        }

        // Apply gain reduction and makeup gain
        float compressed = in[i] * gain_reduction * makeup_gain_linear;

        // Mix dry and wet
        out[i] = in[i] * dry + compressed * mix;
    }
}
