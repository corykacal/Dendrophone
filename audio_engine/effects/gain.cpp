#include "gain.h"
#include <cstdlib>
#include <cstring>

GainState* gain_state_create(float gain, float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    GainState* state = static_cast<GainState*>(
        aligned_alloc(64, align_up(sizeof(GainState))));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;

    state->gain.base = gain;
    state->gain.mod = 0.0f;
    state->gain.smoothed = gain;
    state->gain.set_smoothing(20.0f, sample_rate);  // 20ms smoothing

    return state;
}

void gain_state_destroy(GainState* state) {
    if (!state) return;
    free(state);
}

void gain_op(DSPBlock& b, float* buffers, int n) {
    GainState* s = static_cast<GainState*>(b.state);
    if (!s) {
        float* in  = &buffers[b.in_a * n];
        float* out = &buffers[b.out  * n];
        memcpy(out, in, n * sizeof(float));
        return;
    }

    float* in  = &buffers[b.in_a * n];
    float* out = &buffers[b.out  * n];

    for (int i = 0; i < n; i++) {
        out[i] = in[i] * s->gain.get_smoothed();
    }
}
