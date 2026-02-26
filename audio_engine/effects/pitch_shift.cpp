#include "pitch_shift.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cfloat>

// Round up to the next power of 2 (or return n if already a power of 2).
static uint32_t next_pow2(uint32_t n) {
    n--; n|=n>>1; n|=n>>2; n|=n>>4; n|=n>>8; n|=n>>16; return n+1;
}

PitchShiftState* pitch_shift_state_create(float pitch, float mix, float sample_rate) {
    auto align_up = [](size_t n) -> size_t { return (n + 63) & ~size_t(63); };
    PitchShiftState* state = (PitchShiftState*)aligned_alloc(64, align_up(sizeof(PitchShiftState)));
    if (!state) return nullptr;

    state->sample_rate   = sample_rate;
    state->grain_size    = (uint32_t)(sample_rate * PitchShiftState::GRAIN_MS / 1000.0f);
    state->read_delay    = (uint32_t)(sample_rate * PitchShiftState::READ_DELAY_MS / 1000.0f);
    state->search_delta  = (uint32_t)(sample_rate * PitchShiftState::SEARCH_DELTA_MS / 1000.0f);
    state->corr_len      = state->grain_size / PitchShiftState::CORR_LEN_DIV;
    state->corr_stride   = PitchShiftState::CORR_STRIDE;

    // Buffer: at least 300 ms, rounded to next power of 2 for & mask arithmetic.
    // At 48 kHz: 14400 → 16384 (64 KB). At 44.1 kHz: 13230 → 16384.
    uint32_t min_buf    = (uint32_t)(sample_rate * 0.30f);
    state->buffer_size  = next_pow2(min_buf);
    state->buf_mask     = state->buffer_size - 1;
    state->buffer       = (float*)calloc(state->buffer_size, sizeof(float));
    if (!state->buffer) { free(state); return nullptr; }
    state->write_pos    = 0;

    float pitch_ratio = powf(2.0f, pitch / 12.0f);

    // Magic-circle Hann oscillator: precompute one pair of trig values.
    float inc_angle     = 6.28318530f / (float)state->grain_size;
    state->hann_cos_inc = cosf(inc_angle);
    state->hann_sin_inc = sinf(inc_angle);

    // Grain 0 at phase 0 → envelope = 0 (fade in).
    state->hann_cos[0] =  1.0f;
    state->hann_sin[0] =  0.0f;
    // Grain 1 at phase 0.5 (π) → envelope = 1 (peak). No trig call needed.
    state->hann_cos[1] = -1.0f;
    state->hann_sin[1] =  0.0f;

    // Integer grain counters: grain 0 starts at the beginning of its period,
    // grain 1 starts halfway through so their resets are staggered by grain_size/2.
    state->grain_samples[0] = 0;
    state->grain_samples[1] = state->grain_size / 2;

    // Initial read positions: both anchored to write_pos - read_delay.
    // Grain 1 is offset by the accumulated phase difference over grain_size/2 samples
    // at pitch_ratio, relative to the nominal delay anchor.
    uint32_t nominal    = (state->buffer_size - state->read_delay) & state->buf_mask;
    state->read_pos[0]  = (float)nominal;
    float rp1 = (float)nominal + (float)state->grain_size * 0.5f * pitch_ratio;
    if (rp1 >= (float)state->buffer_size) rp1 -= (float)state->buffer_size;
    if (rp1 < 0.0f)                       rp1 += (float)state->buffer_size;
    state->read_pos[1]  = rp1;

    state->pitch.base = pitch;  state->pitch.smoothed = pitch;  state->pitch.smooth_coeff = 0.001f;
    state->mix.base   = mix;    state->mix.smoothed   = mix;    state->mix.smooth_coeff   = 0.01f;

    return state;
}

void pitch_shift_state_destroy(PitchShiftState* state) {
    if (!state) return;
    free(state->buffer);
    free(state);
}

// 4-point Catmull-Rom cubic interpolation.
// Uses & mask instead of % size — requires buffer_size to be a power of 2.
static inline float read_interp(const float* buf, uint32_t mask, float pos) {
    uint32_t i1 = (uint32_t)pos & mask;
    uint32_t i0 = (i1 - 1) & mask;
    uint32_t i2 = (i1 + 1) & mask;
    uint32_t i3 = (i1 + 2) & mask;
    float t  = pos - floorf(pos);
    float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];
    float a  = 0.5f * (-y0 + 3.0f*y1 - 3.0f*y2 + y3);
    float b  = 0.5f * ( 2.0f*y0 - 5.0f*y1 + 4.0f*y2 - y3);
    float c  = 0.5f * (-y0 + y2);
    return ((a * t + b) * t + c) * t + y1;
}

// Search ±delta samples around `nominal` for the position that best correlates
// with the reference segment starting at `ref_start`. Returns the best position.
// Correlation is computed on every `stride`th sample over `corr_len` samples.
// Skips the search and returns `nominal` when the reference is near silence.
static uint32_t wsola_find_anchor(const float* buf, uint32_t mask,
                                  uint32_t nominal, uint32_t delta,
                                  uint32_t ref_start, uint32_t corr_len,
                                  uint32_t stride) {
    // Reference energy for silence gate
    float ref_energy = 0.0f;
    for (uint32_t k = 0; k < corr_len; k += stride) {
        float s = buf[(ref_start + k) & mask];
        ref_energy += s * s;
    }
    if (ref_energy < 1e-7f) return nominal;  // silence: skip search

    float    best_score = -FLT_MAX;
    uint32_t best_pos   = nominal;

    for (int32_t d = -(int32_t)delta; d <= (int32_t)delta; d++) {
        // (uint32_t)((int32_t)nominal + d) wraps correctly for negative results
        // because 2^32 ≡ 0 (mod buffer_size) when buffer_size is a power of 2.
        uint32_t cand  = (uint32_t)((int32_t)nominal + d) & mask;
        float    score = 0.0f;
        for (uint32_t k = 0; k < corr_len; k += stride)
            score += buf[(ref_start + k) & mask] * buf[(cand + k) & mask];
        if (score > best_score) {
            best_score = score;
            best_pos   = cand;
        }
    }
    return best_pos;
}

void pitch_shift_op(DSPBlock& b, float* buffers, int n) {
    PitchShiftState* s = (PitchShiftState*)b.state;
    float* in  = &buffers[b.in_a * n];
    float* out = &buffers[b.out  * n];

    float pitch_semitones = s->pitch.get_smoothed();
    float mix             = s->mix.get_smoothed();
    pitch_semitones = fminf(fmaxf(pitch_semitones, -12.0f), 12.0f);
    mix             = fminf(fmaxf(mix,              0.0f),   1.0f);
    float pitch_ratio = powf(2.0f, pitch_semitones / 12.0f);

    // Cache hot-path constants in locals to avoid repeated struct dereferences
    const uint32_t mask         = s->buf_mask;
    const uint32_t grain_size   = s->grain_size;
    const uint32_t read_delay   = s->read_delay;
    const uint32_t search_delta = s->search_delta;
    const uint32_t corr_len     = s->corr_len;
    const uint32_t corr_stride  = s->corr_stride;
    const float    cos_inc      = s->hann_cos_inc;
    const float    sin_inc      = s->hann_sin_inc;

    for (int i = 0; i < n; i++) {
        s->buffer[s->write_pos] = in[i];

        float shifted = 0.0f;
        for (int g = 0; g < 2; g++) {
            // 1. Hann envelope from magic-circle oscillator (no trig call)
            float env = 0.5f - 0.5f * s->hann_cos[g];

            // 2. Read fractional sample and accumulate
            shifted += env * read_interp(s->buffer, mask, s->read_pos[g]);

            // 3. Advance read position at pitch_ratio speed
            s->read_pos[g] += pitch_ratio;
            if (s->read_pos[g] >= (float)s->buffer_size)
                s->read_pos[g] -= (float)s->buffer_size;

            // 4. Advance magic-circle Hann oscillator (4 FMAs)
            float nc = s->hann_cos[g] * cos_inc - s->hann_sin[g] * sin_inc;
            float ns = s->hann_sin[g] * cos_inc + s->hann_cos[g] * sin_inc;
            s->hann_cos[g] = nc;
            s->hann_sin[g] = ns;

            // 5. Integer grain counter; WSOLA anchor search at each reset
            if (++s->grain_samples[g] >= grain_size) {
                s->grain_samples[g] = 0;
                // Reinitialize magic circle to phase 0 (exact, no drift)
                s->hann_cos[g] = 1.0f;
                s->hann_sin[g] = 0.0f;
                // Nominal anchor: write_pos - read_delay
                uint32_t nominal   = (s->write_pos + s->buffer_size - read_delay) & mask;
                // Reference: what the other grain is currently reading
                uint32_t ref_start = (uint32_t)s->read_pos[1 - g] & mask;
                s->read_pos[g] = (float)wsola_find_anchor(
                    s->buffer, mask, nominal, search_delta,
                    ref_start, corr_len, corr_stride);
            }
        }

        // Two Hann windows at 50% overlap sum to 1.0 — no gain correction needed
        out[i] = in[i] * (1.0f - mix) + shifted * mix;
        s->write_pos = (s->write_pos + 1) & mask;
    }
}
