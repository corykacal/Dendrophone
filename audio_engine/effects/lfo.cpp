#include "lfo.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LFOState* lfo_state_create(LFOWave wave, float freq_hz, float depth, float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    LFOState* state = static_cast<LFOState*>(aligned_alloc(64, align_up(sizeof(LFOState))));
    if (!state) return nullptr;

    state->wave = wave;
    state->freq_hz = freq_hz;
    state->depth = depth;
    state->phase = 0.0f;
    state->sample_rate = sample_rate;
    state->output = 0.0f;
    state->random_value = 0.0f;
    state->last_phase = 0.0f;

    return state;
}

void lfo_state_destroy(LFOState* state) {
    if (state) free(state);
}

LFOWave lfo_wave_from_string(const std::string& s) {
    if (s == "sine") return LFOWave::Sine;
    if (s == "triangle" || s == "tri") return LFOWave::Triangle;
    if (s == "square" || s == "sq") return LFOWave::Square;
    if (s == "saw" || s == "sawtooth") return LFOWave::Saw;
    if (s == "random" || s == "sh" || s == "s&h") return LFOWave::Random;
    return LFOWave::Sine;  // Default
}

// Simple pseudo-random for S&H (deterministic, RT-safe)
static float simple_random(uint32_t& seed) {
    seed = seed * 1103515245 + 12345;
    return ((seed >> 16) & 0x7FFF) / 32768.0f * 2.0f - 1.0f;
}

void lfo_op(DSPBlock& b, float* buffers, int frames) {
    LFOState* s = static_cast<LFOState*>(b.state);
    if (!s) return;

    // Calculate phase increment for this block
    float phase_inc = s->freq_hz * frames / s->sample_rate;
    s->last_phase = s->phase;
    s->phase = fmodf(s->phase + phase_inc, 1.0f);

    // Generate waveform value at current phase
    float raw = 0.0f;

    switch (s->wave) {
        case LFOWave::Sine:
            raw = sinf(2.0f * M_PI * s->phase);
            break;

        case LFOWave::Triangle:
            // Triangle: 0→1→0→-1→0 over one cycle
            if (s->phase < 0.25f) {
                raw = s->phase * 4.0f;
            } else if (s->phase < 0.75f) {
                raw = 1.0f - (s->phase - 0.25f) * 4.0f;
            } else {
                raw = -1.0f + (s->phase - 0.75f) * 4.0f;
            }
            break;

        case LFOWave::Square:
            raw = (s->phase < 0.5f) ? 1.0f : -1.0f;
            break;

        case LFOWave::Saw:
            // Saw: -1 to +1 over cycle
            raw = s->phase * 2.0f - 1.0f;
            break;

        case LFOWave::Random:
            // Sample & hold: new random value when phase wraps
            if (s->phase < s->last_phase) {
                // Phase wrapped - generate new random value
                static uint32_t seed = 12345;
                s->random_value = simple_random(seed);
            }
            raw = s->random_value;
            break;
    }

    // Apply depth and store output
    s->output = raw * s->depth;

    // Write to output buffer (control buffer at b.out)
    // For control signals, we write a single value
    buffers[b.out] = s->output;
}
