#include "granular.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>

// Helper: Generate window function lookup table
static void generate_window(float* table, int size, GranularState::WindowType type) {
    for (int i = 0; i < size; i++) {
        float t = static_cast<float>(i) / (size - 1);  // 0.0 to 1.0

        switch (type) {
            case GranularState::WindowType::Hann:
                // Hann window: 0.5 * (1 - cos(2πt))
                table[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * t));
                break;

            case GranularState::WindowType::Blackman:
                // Blackman window
                table[i] = 0.42f - 0.5f * cosf(2.0f * M_PI * t) + 0.08f * cosf(4.0f * M_PI * t);
                break;

            case GranularState::WindowType::Triangle:
                // Triangle window
                table[i] = 1.0f - fabsf(2.0f * t - 1.0f);
                break;

            case GranularState::WindowType::Tukey:
                // Tukey window (alpha = 0.5)
                {
                    float alpha = 0.5f;
                    if (t < alpha / 2.0f) {
                        table[i] = 0.5f * (1.0f + cosf(M_PI * (2.0f * t / alpha - 1.0f)));
                    } else if (t > 1.0f - alpha / 2.0f) {
                        table[i] = 0.5f * (1.0f + cosf(M_PI * (2.0f * t / alpha - 2.0f / alpha + 1.0f)));
                    } else {
                        table[i] = 1.0f;
                    }
                }
                break;

            default:
                table[i] = 1.0f;
                break;
        }
    }
}

// Simple random number generator (LCG)
static inline float random_float(uint32_t& seed) {
    seed = seed * 1103515245 + 12345;
    return static_cast<float>((seed / 65536) % 32768) / 32768.0f;
}

GranularState::WindowType granular_window_from_string(const std::string& window_str) {
    if (window_str == "hann") return GranularState::WindowType::Hann;
    if (window_str == "blackman") return GranularState::WindowType::Blackman;
    if (window_str == "triangle") return GranularState::WindowType::Triangle;
    if (window_str == "tukey") return GranularState::WindowType::Tukey;
    return GranularState::WindowType::Hann;  // Default
}

GranularState* granular_state_create(float grain_size_ms,
                                      float density,
                                      float pitch_shift,
                                      float pitch_random,
                                      float position_random,
                                      GranularState::WindowType window_type,
                                      float mix,
                                      float sample_rate) {
    GranularState* state = static_cast<GranularState*>(
        aligned_alloc(64, sizeof(GranularState)));
    if (!state) return nullptr;

    state->sample_rate = sample_rate;
    state->window_type = window_type;

    // Allocate source buffer (2 seconds)
    state->buffer_size = static_cast<uint32_t>(sample_rate * 2.0f);
    state->buffer = static_cast<float*>(
        aligned_alloc(64, state->buffer_size * sizeof(float)));
    if (!state->buffer) {
        free(state);
        return nullptr;
    }
    memset(state->buffer, 0, state->buffer_size * sizeof(float));
    state->write_pos = 0;

    // Allocate and generate window table
    state->window_table = static_cast<float*>(
        aligned_alloc(64, GranularState::WINDOW_TABLE_SIZE * sizeof(float)));
    if (!state->window_table) {
        free(state->buffer);
        free(state);
        return nullptr;
    }
    generate_window(state->window_table, GranularState::WINDOW_TABLE_SIZE, window_type);

    // Initialize grains
    for (int i = 0; i < GranularState::MAX_GRAINS; i++) {
        state->grains[i].active = false;
        state->grains[i].phase = 0.0f;
        state->grains[i].phase_inc = 0.0f;
        state->grains[i].buffer_pos = 0.0f;
        state->grains[i].pitch_ratio = 1.0f;
        state->grains[i].amplitude = 1.0f;
        state->grains[i].length_samples = 0;
    }
    state->num_active_grains = 0;

    // Initialize grain scheduling
    state->grain_spawn_counter = 0.0f;
    state->random_seed = 12345;

    // Initialize parameters
    state->grain_size_ms.base = grain_size_ms;
    state->grain_size_ms.smoothed = grain_size_ms;
    state->grain_size_ms.set_smoothing(20.0f, sample_rate);

    state->density.base = density;
    state->density.smoothed = density;
    state->density.set_smoothing(30.0f, sample_rate);

    state->pitch_shift.base = pitch_shift;
    state->pitch_shift.smoothed = pitch_shift;
    state->pitch_shift.set_smoothing(10.0f, sample_rate);

    state->pitch_random.base = pitch_random;
    state->pitch_random.smoothed = pitch_random;
    state->pitch_random.smooth_coeff = 0.0f;

    state->position_random.base = position_random;
    state->position_random.smoothed = position_random;
    state->position_random.smooth_coeff = 0.0f;

    state->mix.base = mix;
    state->mix.smoothed = mix;
    state->mix.smooth_coeff = 0.0f;

    return state;
}

void granular_state_destroy(GranularState* state) {
    if (!state) return;
    if (state->buffer) free(state->buffer);
    if (state->window_table) free(state->window_table);
    free(state);
}

// Spawn a new grain
static void spawn_grain(GranularState* s, float grain_size_ms, float pitch_shift_st,
                        float pitch_random_st, float position_random_ms) {
    // Find inactive grain slot
    int slot = -1;
    for (int i = 0; i < GranularState::MAX_GRAINS; i++) {
        if (!s->grains[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return;  // All grains active

    GranularState::Grain& grain = s->grains[slot];
    grain.active = true;
    grain.phase = 0.0f;

    // Calculate grain length
    uint32_t grain_length = static_cast<uint32_t>(
        grain_size_ms * s->sample_rate / 1000.0f);
    grain_length = std::clamp(grain_length, 240u, 48000u);  // 5ms to 1000ms
    grain.length_samples = grain_length;
    grain.phase_inc = 1.0f / grain_length;

    // Calculate pitch ratio with randomization
    float pitch_variation = (random_float(s->random_seed) * 2.0f - 1.0f) * pitch_random_st;
    float total_pitch = pitch_shift_st + pitch_variation;
    grain.pitch_ratio = powf(2.0f, total_pitch / 12.0f);

    // Calculate buffer position with randomization
    float position_jitter = (random_float(s->random_seed) * 2.0f - 1.0f) * position_random_ms;
    int32_t jitter_samples = static_cast<int32_t>(position_jitter * s->sample_rate / 1000.0f);
    int32_t buffer_offset = static_cast<int32_t>(s->write_pos) + jitter_samples;

    // Wrap buffer position
    while (buffer_offset < 0) buffer_offset += s->buffer_size;
    while (buffer_offset >= static_cast<int32_t>(s->buffer_size)) {
        buffer_offset -= s->buffer_size;
    }

    grain.buffer_pos = static_cast<float>(buffer_offset);
    grain.amplitude = 0.7f;  // Amplitude per grain (will be windowed)

    s->num_active_grains++;
}

void granular_op(DSPBlock& b, float* buffers, int n) {
    GranularState* s = static_cast<GranularState*>(b.state);
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
    float grain_size_ms = std::clamp(s->grain_size_ms.get_smoothed(), 5.0f, 200.0f);
    float density = std::clamp(s->density.get_smoothed(), 1.0f, 50.0f);
    float pitch_shift = std::clamp(s->pitch_shift.get_smoothed(), -12.0f, 12.0f);
    float pitch_random = std::clamp(s->pitch_random.get(), 0.0f, 12.0f);
    float position_random = std::clamp(s->position_random.get(), 0.0f, 500.0f);

    // Calculate grain spawn interval
    float grains_per_sample = density / s->sample_rate;

    for (int i = 0; i < n; i++) {
        // Write input to buffer
        s->buffer[s->write_pos] = in[i];

        // Grain scheduling - spawn new grains based on density
        s->grain_spawn_counter += grains_per_sample;
        if (s->grain_spawn_counter >= 1.0f) {
            s->grain_spawn_counter -= 1.0f;
            spawn_grain(s, grain_size_ms, pitch_shift, pitch_random, position_random);
        }

        // Process all active grains
        float grain_sum = 0.0f;
        s->num_active_grains = 0;

        for (int g = 0; g < GranularState::MAX_GRAINS; g++) {
            GranularState::Grain& grain = s->grains[g];
            if (!grain.active) continue;

            s->num_active_grains++;

            // Read from buffer with interpolation
            uint32_t read_idx = static_cast<uint32_t>(grain.buffer_pos) % s->buffer_size;
            uint32_t read_idx_next = (read_idx + 1) % s->buffer_size;
            float frac = grain.buffer_pos - floorf(grain.buffer_pos);

            float sample_a = s->buffer[read_idx];
            float sample_b = s->buffer[read_idx_next];
            float grain_sample = sample_a * (1.0f - frac) + sample_b * frac;

            // Apply window envelope
            uint32_t window_idx = static_cast<uint32_t>(
                grain.phase * (GranularState::WINDOW_TABLE_SIZE - 1));
            float window_val = s->window_table[window_idx];

            // Add windowed grain to output
            grain_sum += grain_sample * window_val * grain.amplitude;

            // Advance grain
            grain.phase += grain.phase_inc;
            grain.buffer_pos -= grain.pitch_ratio;  // Read backward for natural sound

            // Wrap buffer position
            if (grain.buffer_pos < 0.0f) {
                grain.buffer_pos += s->buffer_size;
            }

            // Deactivate grain when complete
            if (grain.phase >= 1.0f) {
                grain.active = false;
                s->num_active_grains--;
            }
        }

        // Normalize by max possible grains to prevent clipping
        float normalization = 1.0f / sqrtf(GranularState::MAX_GRAINS);

        // Mix dry/wet
        out[i] = in[i] * dry + grain_sum * mix * normalization;

        // Advance write position
        s->write_pos = (s->write_pos + 1) % s->buffer_size;
    }
}
