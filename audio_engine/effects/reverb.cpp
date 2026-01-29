#include "reverb.h"
#include <cstdlib>
#include <cstring>
#include <cmath>

// Prime numbers for comb delay times (in samples at 48kHz)
// These create a natural reverb sound by avoiding harmonic relationships
static const uint32_t COMB_DELAYS_48K[ReverbState::NUM_COMBS] = {
    1557, 1617, 1491, 1422  // ~32ms, ~33ms, ~31ms, ~29ms
};

static const uint32_t ALLPASS_DELAYS_48K[ReverbState::NUM_ALLPASSES] = {
    225, 341  // ~4.7ms, ~7.1ms
};

ReverbState* reverb_state_create(float decay, float mix, float room_size, float sample_rate) {
    ReverbState* state = (ReverbState*)aligned_alloc(64, sizeof(ReverbState));
    if (!state) return nullptr;

    // Initialize all fields to zero
    for (int i = 0; i < ReverbState::NUM_COMBS; i++) {
        state->comb_buffers[i] = nullptr;
        state->comb_sizes[i] = 0;
        state->comb_positions[i] = 0;
    }
    for (int i = 0; i < ReverbState::NUM_ALLPASSES; i++) {
        state->allpass_buffers[i] = nullptr;
        state->allpass_sizes[i] = 0;
        state->allpass_positions[i] = 0;
    }
    state->sample_rate = sample_rate;

    // Scale delay times based on sample rate
    float scale = sample_rate / 48000.0f;

    // Initialize comb filters
    for (int i = 0; i < ReverbState::NUM_COMBS; i++) {
        state->comb_sizes[i] = (uint32_t)(COMB_DELAYS_48K[i] * scale);
        state->comb_buffers[i] = (float*)calloc(state->comb_sizes[i], sizeof(float));
        state->comb_positions[i] = 0;
    }

    // Initialize allpass filters
    for (int i = 0; i < ReverbState::NUM_ALLPASSES; i++) {
        state->allpass_sizes[i] = (uint32_t)(ALLPASS_DELAYS_48K[i] * scale);
        state->allpass_buffers[i] = (float*)calloc(state->allpass_sizes[i], sizeof(float));
        state->allpass_positions[i] = 0;
    }

    // Initialize parameters
    state->decay.base = decay;
    state->decay.smoothed = decay;
    state->decay.smooth_coeff = 0.01f;

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.01f;

    state->room_size.base = room_size;
    state->room_size.smoothed = room_size;
    state->room_size.smooth_coeff = 0.001f;  // Slower smoothing for room size

    return state;
}

void reverb_state_destroy(ReverbState* state) {
    if (!state) return;

    for (int i = 0; i < ReverbState::NUM_COMBS; i++) {
        free(state->comb_buffers[i]);
    }

    for (int i = 0; i < ReverbState::NUM_ALLPASSES; i++) {
        free(state->allpass_buffers[i]);
    }

    free(state);
}

void reverb_op(DSPBlock& b, float* buffers, int n) {
    ReverbState* s = (ReverbState*)b.state;
    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    // Get smoothed parameters
    float decay = s->decay.get_smoothed();
    float mix = s->mix.get_smoothed();
    float room_size = s->room_size.get_smoothed();

    // Clamp parameters
    decay = fminf(fmaxf(decay, 0.0f), 0.99f);  // Prevent runaway feedback
    mix = fminf(fmaxf(mix, 0.0f), 1.0f);
    room_size = fminf(fmaxf(room_size, 0.3f), 1.0f);  // Min 30% to prevent tiny delays

    // Process each sample
    for (int i = 0; i < n; i++) {
        float input = in[i];
        float comb_sum = 0.0f;

        // Parallel comb filters
        for (int c = 0; c < ReverbState::NUM_COMBS; c++) {
            // Scale delay time based on room size
            uint32_t delay = (uint32_t)(s->comb_sizes[c] * room_size);
            if (delay >= s->comb_sizes[c]) delay = s->comb_sizes[c] - 1;

            uint32_t read_pos = (s->comb_positions[c] + s->comb_sizes[c] - delay) % s->comb_sizes[c];
            float delayed = s->comb_buffers[c][read_pos];

            // Feedback with decay
            float output = input + delayed * decay;
            s->comb_buffers[c][s->comb_positions[c]] = output;

            comb_sum += delayed;
            s->comb_positions[c] = (s->comb_positions[c] + 1) % s->comb_sizes[c];
        }

        // Average comb outputs
        float verb = comb_sum / ReverbState::NUM_COMBS;

        // Series allpass filters (diffusion)
        for (int a = 0; a < ReverbState::NUM_ALLPASSES; a++) {
            float delayed = s->allpass_buffers[a][s->allpass_positions[a]];
            float allpass_out = -verb + delayed;
            s->allpass_buffers[a][s->allpass_positions[a]] = verb + delayed * 0.5f;
            verb = allpass_out;
            s->allpass_positions[a] = (s->allpass_positions[a] + 1) % s->allpass_sizes[a];
        }

        // Mix dry/wet
        out[i] = input * (1.0f - mix) + verb * mix;
    }
}
