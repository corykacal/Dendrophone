#include "reverb.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

// Freeverb delay times at 48kHz (all prime, 8 combs + 4 allpasses)
static const uint32_t COMB_DELAYS_48K[ReverbState::NUM_COMBS] = {
    1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617
};

static const uint32_t ALLPASS_DELAYS_48K[ReverbState::NUM_ALLPASSES] = {
    556, 441, 341, 225
};

ReverbState* reverb_state_create(float decay, float mix, float room_size,
                                  float damping, float pre_delay_ms,
                                  float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    ReverbState* state = (ReverbState*)aligned_alloc(64, align_up(sizeof(ReverbState)));
    if (!state) return nullptr;

    // Initialize all fields to zero
    for (int i = 0; i < ReverbState::NUM_COMBS; i++) {
        state->comb_buffers[i] = nullptr;
        state->comb_sizes[i] = 0;
        state->comb_positions[i] = 0;
    }
    memset(state->comb_filter_state, 0, sizeof(state->comb_filter_state));

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

    // Pre-delay buffer (max 50ms)
    state->pre_delay_buffer_size = (uint32_t)(sample_rate * 0.05f);
    state->pre_delay_buffer = (float*)calloc(state->pre_delay_buffer_size, sizeof(float));
    state->pre_delay_write_pos = 0;

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

    state->damping.base = damping;
    state->damping.smoothed = damping;
    state->damping.smooth_coeff = 0.01f;

    state->pre_delay_ms.base = pre_delay_ms;
    state->pre_delay_ms.smoothed = pre_delay_ms;
    state->pre_delay_ms.smooth_coeff = 0.001f;  // Slow to avoid buffer glitch

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

    free(state->pre_delay_buffer);
    free(state);
}

void reverb_op(DSPBlock& b, float* buffers, int n) {
    ReverbState* s = (ReverbState*)b.state;
    float* in = &buffers[b.in_a * n];
    float* out = &buffers[b.out * n];

    // Get smoothed parameters
    float decay     = s->decay.get_smoothed();
    float mix       = s->mix.get_smoothed();
    float room_size = s->room_size.get_smoothed();
    float damping   = s->damping.get_smoothed();
    float pre_delay = s->pre_delay_ms.get_smoothed();

    // Clamp parameters
    decay     = fminf(fmaxf(decay,     0.0f), 0.99f);  // Prevent runaway feedback
    mix       = fminf(fmaxf(mix,       0.0f), 1.0f);
    room_size = fminf(fmaxf(room_size, 0.3f), 1.0f);   // Min 30% to prevent tiny delays
    damping   = fminf(fmaxf(damping,   0.0f), 1.0f);
    pre_delay = fminf(fmaxf(pre_delay, 0.0f), 50.0f);

    // Process each sample
    for (int i = 0; i < n; i++) {
        float input = in[i];

        // Pre-delay: write input, read back delayed_input
        uint32_t pd_samples = (uint32_t)(pre_delay * s->sample_rate / 1000.0f);
        pd_samples = std::min(pd_samples, s->pre_delay_buffer_size - 1);
        uint32_t pd_read = (s->pre_delay_write_pos + s->pre_delay_buffer_size - pd_samples)
                           % s->pre_delay_buffer_size;
        float delayed_input = s->pre_delay_buffer[pd_read];
        s->pre_delay_buffer[s->pre_delay_write_pos] = input;
        s->pre_delay_write_pos = (s->pre_delay_write_pos + 1) % s->pre_delay_buffer_size;

        // Parallel comb filters with one-pole LPF damping in feedback path
        float comb_sum = 0.0f;
        for (int c = 0; c < ReverbState::NUM_COMBS; c++) {
            uint32_t delay = (uint32_t)(s->comb_sizes[c] * room_size);
            if (delay >= s->comb_sizes[c]) delay = s->comb_sizes[c] - 1;

            uint32_t read_pos = (s->comb_positions[c] + s->comb_sizes[c] - delay)
                                % s->comb_sizes[c];
            float delayed = s->comb_buffers[c][read_pos];

            // One-pole LPF: absorbs high frequencies on each reflection
            float damped = delayed * (1.0f - damping) + s->comb_filter_state[c] * damping;
            s->comb_filter_state[c] = damped;

            s->comb_buffers[c][s->comb_positions[c]] = delayed_input + damped * decay;
            comb_sum += delayed;
            s->comb_positions[c] = (s->comb_positions[c] + 1) % s->comb_sizes[c];
        }

        float verb = comb_sum / ReverbState::NUM_COMBS;

        // Series allpass filters — correct Moorer form with g=0.5
        for (int a = 0; a < ReverbState::NUM_ALLPASSES; a++) {
            float del = s->allpass_buffers[a][s->allpass_positions[a]];
            float v   = verb + 0.5f * del;
            verb      = del - 0.5f * v;
            s->allpass_buffers[a][s->allpass_positions[a]] = v;
            s->allpass_positions[a] = (s->allpass_positions[a] + 1) % s->allpass_sizes[a];
        }

        // Mix dry/wet
        out[i] = input * (1.0f - mix) + verb * mix;
    }
}
