# Dendrophone Effects Reference

This document describes all available audio effects in the Dendrophone audio engine.

## Available Effects

### 1. Delay

A simple delay/echo effect with feedback control.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `time_ms` (float) - Delay time in milliseconds (default: 250.0)
- `feedback` (float) - Feedback amount 0.0 to 1.0 (default: 0.0)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `time_ms`, `feedback`, `mix`

**Example:**
```json
{
  "delay1": {
    "type": "delay",
    "inputs": ["in"],
    "outputs": ["out"],
    "param_inputs": ["time_ms"],
    "params": {
      "time_ms": 300.0,
      "feedback": 0.4,
      "mix": 0.5
    }
  }
}
```

---

### 2. Reverb

Schroeder reverb using parallel comb filters and series allpass filters.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `decay` (float) - Decay time 0.0 to 1.0 (default: 0.6)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 0.5)
- `room_size` (float) - Room size scaling 0.0 to 1.0 (default: 0.8)

**Modulatable:** `decay`, `mix`, `room_size`

**Example:**
```json
{
  "reverb1": {
    "type": "reverb",
    "inputs": ["in"],
    "outputs": ["out"],
    "param_inputs": ["decay", "room_size"],
    "params": {
      "decay": 0.7,
      "mix": 0.4,
      "room_size": 0.8
    }
  }
}
```

**Implementation Details:**
- Uses 4 comb filters in parallel with prime-numbered delay times
- Uses 2 allpass filters in series for diffusion
- Automatically scales delay times based on sample rate
- Room size parameter scales all delay times proportionally

---

### 3. Pitch Shift

Simple pitch shifter using dual delay lines with crossfading.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `pitch` (float) - Pitch shift in semitones, -12.0 to +12.0 (default: 0.0)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `pitch`, `mix`

**Example:**
```json
{
  "pitch1": {
    "type": "pitch_shift",
    "inputs": ["in"],
    "outputs": ["out"],
    "param_inputs": ["pitch"],
    "params": {
      "pitch": 5.0,
      "mix": 0.7
    }
  }
}
```

**Implementation Details:**
- Uses two read heads with 50ms crossfade window
- Playback rate adjusted by 2^(semitones/12)
- 200ms internal buffer for delay line
- Linear interpolation for smooth playback

---

### 4. State Variable Filter (SVFilter)

Resonant filter with multiple modes (lowpass, highpass, bandpass, notch).

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `mode` (string) - Filter mode: "lowpass"/"lp", "highpass"/"hp", "bandpass"/"bp", "notch" (default: "lowpass")
- `cutoff` (float) - Cutoff frequency in Hz, 20.0 to 20000.0 (default: 1000.0)
- `resonance` (float) - Resonance/Q factor, 0.5 to 10.0 (default: 1.0, self-oscillates at ~10)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `cutoff`, `resonance`, `mix`

**Example:**
```json
{
  "filter1": {
    "type": "svfilter",
    "inputs": ["in"],
    "outputs": ["out"],
    "param_inputs": ["cutoff", "resonance"],
    "params": {
      "mode": "lowpass",
      "cutoff": 1000.0,
      "resonance": 2.0,
      "mix": 1.0
    }
  }
}
```

**Implementation Details:**
- State Variable Filter topology for smooth parameter modulation
- Automatically clamped to safe ranges (cutoff: 10 Hz to Nyquist/2, resonance: 0.1 to 12.0)
- 5ms smoothing on cutoff, 10ms on resonance to prevent clicks
- Stable at extreme resonance values

---

### 5. Reverse Playback

Reverse audio playback with smooth crossfading and forward/reverse morphing.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `buffer_time_ms` (float) - Reverse buffer length in milliseconds, 50.0 to 2000.0 (default: 500.0)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)
- `enabled` (float) - Reverse amount, 0.0 (forward) to 1.0 (reverse) (default: 1.0)

**Modulatable:** `buffer_time_ms`, `mix`, `enabled`

**Example:**
```json
{
  "reverse1": {
    "type": "reverse",
    "inputs": ["in"],
    "outputs": ["out"],
    "param_inputs": ["enabled"],
    "params": {
      "buffer_time_ms": 500.0,
      "mix": 1.0,
      "enabled": 1.0
    }
  }
}
```

**Implementation Details:**
- Fixed-length reverse buffer with backward reading
- 10ms cosine crossfade at loop boundaries for smooth transitions
- Linear interpolation for sub-sample accuracy
- `enabled` parameter allows smooth morphing between forward and reverse
- 50ms smoothing on buffer size changes to prevent clicks

---

### 6. Multi-Tap Delay

Multi-tap delay with up to 8 independent delay taps, each with its own timing and gain.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `tap0_time_ms` through `tap7_time_ms` (float) - Delay time for each tap in milliseconds, 0.0 to 2000.0
- `tap0_gain` through `tap7_gain` (float) - Gain for each tap, 0.0 to 2.0
- `feedback` (float) - Global feedback amount, 0.0 to 0.99 (default: 0.3)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** All tap times and gains, `feedback`, `mix`

**Example:**
```json
{
  "multitap1": {
    "type": "multitap_delay",
    "inputs": ["in"],
    "outputs": ["out"],
    "params": {
      "tap0_time_ms": 150.0,
      "tap0_gain": 0.8,
      "tap1_time_ms": 300.0,
      "tap1_gain": 0.6,
      "tap2_time_ms": 450.0,
      "tap2_gain": 0.4,
      "feedback": 0.3,
      "mix": 0.5
    }
  }
}
```

**Implementation Details:**
- Single shared ring buffer for memory efficiency
- Up to 8 independent read heads with per-tap timing and gain
- Global feedback sums all taps before feeding back to input
- 5ms smoothing on tap times to prevent clicks
- Foundation for granular synthesis (taps can become grains)

---

### 7. Granular Synthesis

Full-featured granular synthesis engine with up to 16 simultaneous grains and multiple window functions.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `grain_size_ms` (float) - Grain length in milliseconds, 5.0 to 200.0 (default: 50.0)
- `density` (float) - Grains spawned per second, 1.0 to 50.0 (default: 10.0)
- `pitch_shift` (float) - Pitch shift in semitones, -12.0 to 12.0 (default: 0.0)
- `pitch_random` (float) - Random pitch variation per grain in semitones, 0.0 to 12.0 (default: 0.0)
- `position_random` (float) - Random buffer position jitter in milliseconds, 0.0 to 500.0 (default: 50.0)
- `window` (string) - Grain envelope: "hann", "blackman", "triangle", "tukey" (default: "hann")
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `grain_size_ms`, `density`, `pitch_shift`, `pitch_random`, `position_random`, `mix`

**Example:**
```json
{
  "granular1": {
    "type": "granular",
    "inputs": ["in"],
    "outputs": ["out"],
    "param_inputs": ["grain_size_ms", "pitch_shift"],
    "params": {
      "grain_size_ms": 50.0,
      "density": 15.0,
      "pitch_shift": 0.0,
      "pitch_random": 2.0,
      "position_random": 100.0,
      "window": "hann",
      "mix": 1.0
    }
  }
}
```

**Implementation Details:**
- Up to 16 simultaneous grains for dense textures
- 4 window functions: Hann (smooth), Blackman (extra smooth), Triangle (sharp), Tukey (flat top)
- 1024-sample window lookup table for efficient envelope shaping
- Grain scheduling with density-based spawn rate
- Per-grain pitch randomization for cloud textures
- Position randomization for buffer exploration
- Linear interpolation for smooth pitch shifting
- Automatic normalization to prevent clipping

**Use Cases:**
- **Freeze effects**: High density (30+), small position random, matched pitch
- **Cloud textures**: Medium density (15-25), high pitch/position random
- **Shimmer**: Pitch shift +7/+12 semitones, medium-large grains
- **Granular reverb**: Small grains (10-30ms), high density, pitch variation

---

### 8. Phrase Looper

60-second phrase looper with record, playback, and overdub modes.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `speed` (float) - Playback speed, 0.25 to 4.0 (default: 1.0)
- `feedback` (float) - Overdub feedback amount, 0.0 to 1.0 (default: 0.8)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)
- `record_trigger` (float) - Trigger recording mode, 0.0 (off) to 1.0 (on) (default: 0.0)
- `play_trigger` (float) - Trigger playback mode, 0.0 (off) to 1.0 (on) (default: 0.0)
- `overdub_trigger` (float) - Trigger overdub mode, 0.0 (off) to 1.0 (on) (default: 0.0)
- `clear_trigger` (float) - Clear loop buffer, 0.0 (off) to 1.0 (on) (default: 0.0)

**Modulatable:** `speed`, `feedback`, `mix`, `record_trigger`, `play_trigger`, `overdub_trigger`, `clear_trigger`

**Example:**
```json
{
  "looper1": {
    "type": "looper",
    "inputs": ["in"],
    "outputs": ["out"],
    "param_inputs": ["speed"],
    "params": {
      "speed": 1.0,
      "feedback": 0.7,
      "mix": 1.0,
      "record_trigger": 0.0,
      "play_trigger": 0.0,
      "overdub_trigger": 0.0,
      "clear_trigger": 0.0
    }
  }
}
```

**Implementation Details:**
- 60-second buffer (2,880,000 samples at 48 kHz)
- Four modes: Bypass, Recording, Playing, Overdubbing
- State machine controlled by trigger parameters
- Speed control with linear interpolation for smooth playback
- Feedback parameter controls overdub mix (lower = clearer overdubs)

**Operation:**
1. Set `record_trigger` to 1.0 to start recording
2. Set `record_trigger` back to 1.0 again to stop recording and start playback
3. Set `play_trigger` to 1.0 to play the recorded loop
4. Set `overdub_trigger` to 1.0 to layer new material over the loop
5. Set `clear_trigger` to 1.0 to clear the buffer and reset

**Use Cases:**
- Live looping for performance
- Creating layered textures with overdubs
- Speed-shifted loop playback for pitch effects
- Foundation for complex loop-based compositions

---

### 9. Glitch/Stutter

Buffer freeze and rhythmic retrigger effect for glitch-style audio manipulation.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `freeze` (float) - Freeze on/off, 0.0 (live passthrough) to 1.0 (frozen stutter) (default: 0.0)
- `stutter_rate_hz` (float) - Retrigger rate in Hz, 0.5 to 50.0 (default: 4.0)
- `capture_size_ms` (float) - Freeze buffer size in milliseconds, 10.0 to 1000.0 (default: 200.0)
- `speed` (float) - Playback speed during freeze, 0.25 to 4.0 (default: 1.0)
- `randomize` (float) - Randomization amount for retrigger point and speed, 0.0 to 1.0 (default: 0.0)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `freeze`, `stutter_rate_hz`, `capture_size_ms`, `speed`, `randomize`, `mix`

**Example:**
```json
{
  "glitch1": {
    "type": "glitch",
    "inputs": ["in"],
    "outputs": ["out"],
    "param_inputs": ["freeze", "stutter_rate_hz"],
    "params": {
      "freeze": 0.0,
      "stutter_rate_hz": 8.0,
      "capture_size_ms": 150.0,
      "speed": 1.0,
      "randomize": 0.5,
      "mix": 1.0
    }
  }
}
```

**Implementation Details:**
- Two modes: live passthrough (freeze < 0.5) and frozen stutter (freeze ≥ 0.5)
- When entering freeze mode, captures current buffer
- Rhythmic retrigger based on stutter_rate_hz
- Randomization affects both retrigger point and playback speed
- Linear interpolation for smooth playback during freeze
- Simple LCG-based random number generator for deterministic randomization

**Operation:**
1. Set `freeze` to 0.0 for normal passthrough (buffer continuously writes)
2. Set `freeze` to 1.0 to capture buffer and start stuttering
3. `stutter_rate_hz` controls how often the buffer retrigggers
4. `randomize` adds variation to retrigger points and speed
5. Set `freeze` back to 0.0 to return to live audio

**Use Cases:**
- Rhythmic glitch effects with stutter patterns
- Buffer freeze for live manipulation
- Randomized textures with controlled chaos
- Synchronized stutters (LFO-modulated freeze parameter)

---

### 10. LFO (Low Frequency Oscillator)

Control-rate signal generator for modulation.

**Ports:**
- `value` - Control output

**Parameters:**
- `wave` (string) - Waveform type: "sine", "triangle", "square", "saw" (default: "sine")
- `freq_hz` (float) - Frequency in Hz (default: 1.0)
- `depth` (float) - Output amplitude scaling (default: 1.0)

**Not Modulatable** (control-rate node)

**Example:**
```json
{
  "lfo1": {
    "type": "lfo",
    "rate": "control",
    "outputs": ["value"],
    "params": {
      "wave": "sine",
      "freq_hz": 0.5,
      "depth": 3.0
    }
  }
}
```

**Supported Waveforms:**
- `sine` - Smooth sinusoidal oscillation
- `triangle` - Linear ramp up and down
- `square` - Binary on/off signal
- `saw` - Linear ramp up, instant drop

---

### 11. Chorus

Lush modulation effect that creates an ensemble-like sound using multiple modulated delay lines.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `rate_hz` (float) - LFO rate in Hz, 0.1 to 10.0 (default: 1.5)
- `depth` (float) - Modulation depth, 0.0 to 1.0 (default: 0.5)
- `voices` (int) - Number of chorus voices, 2 to 4 (default: 3)
- `stereo_width` (float) - Stereo spread, 0.0 to 1.0 (default: 0.5)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 0.5)

**Modulatable:** `rate_hz`, `depth`, `mix`

**Implementation Details:**
- Uses 2-4 independent delay lines (~15-30ms each)
- Each voice has slightly different LFO phase for rich stereo image
- Sine wave LFO modulation creates smooth, classic chorus sound

---

### 12. Tremolo

Amplitude modulation effect that creates rhythmic volume variations.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `rate_hz` (float) - LFO rate in Hz, 0.1 to 20.0 (default: 4.0)
- `depth` (float) - Modulation depth, 0.0 to 1.0 (default: 0.5)
- `waveform` (string) - "sine", "triangle", "square" (default: "sine")
- `stereo_phase` (float) - L/R phase offset, 0.0 to 1.0 (default: 0.0)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `rate_hz`, `depth`, `mix`

**Implementation Details:**
- Simple LFO-controlled gain modulation
- Three waveform options for different tremolo characters
- Extremely CPU-efficient

---

### 13. Flanger

Classic swooshing effect using a short modulated delay with feedback.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `rate_hz` (float) - LFO rate in Hz, 0.1 to 10.0 (default: 0.5)
- `depth` (float) - Delay modulation depth, 0.0 to 1.0 (default: 0.7)
- `feedback` (float) - Feedback amount, -0.95 to 0.95 (default: 0.5)
- `delay_ms` (float) - Center delay time in ms, 0.1 to 10.0 (default: 2.0)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 0.5)

**Modulatable:** `rate_hz`, `depth`, `feedback`, `delay_ms`, `mix`

**Implementation Details:**
- Very short delay (0.1-10ms) with sine LFO modulation
- Feedback path creates classic "jet plane" swoosh
- Negative feedback values create through-zero flanging

---

### 14. Compressor

Dynamics processor that reduces dynamic range for consistent levels and enhanced sustain.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `threshold_db` (float) - Compression threshold in dB, -60.0 to 0.0 (default: -20.0)
- `ratio` (float) - Compression ratio, 1.0 to 20.0 (default: 4.0)
- `attack_ms` (float) - Attack time in ms, 0.1 to 100.0 (default: 10.0)
- `release_ms` (float) - Release time in ms, 10.0 to 1000.0 (default: 100.0)
- `makeup_gain_db` (float) - Post-compression gain in dB, 0.0 to 40.0 (default: 0.0)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `threshold_db`, `ratio`, `attack_ms`, `release_ms`, `makeup_gain_db`, `mix`

**Implementation Details:**
- Feed-forward compressor with peak detection
- Exponential attack/release envelopes
- Logarithmic gain reduction calculation
- Essential for controlling dynamics and adding sustain

---

### 15. Overdrive

Saturation effect that adds harmonic richness through waveshaping.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `drive` (float) - Drive amount, 0.0 to 1.0 (default: 0.5, maps to 1x-50x pre-gain)
- `tone` (float) - Tone control (LP filter), 0.0 to 1.0 (default: 0.5)
- `level` (float) - Output level, 0.0 to 2.0 (default: 1.0)
- `type` (string) - "soft", "hard", "asymmetric" (default: "soft")
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `drive`, `tone`, `level`, `mix`

**Implementation Details:**
- Three waveshaping algorithms:
  - Soft: tanh-style smooth clipping
  - Hard: hard clipping at ±1.0
  - Asymmetric: different clipping curves for positive/negative
- Tone control: 1-pole lowpass filter (500Hz - 5kHz)
- Pre-gain, waveshaping, filtering, output level

---

### 16. Auto-Wah

Envelope-controlled filter that creates dynamic "wah-wah" tones responding to playing dynamics.

**Ports:**
- `in` - Audio input
- `out` - Audio output

**Parameters:**
- `sensitivity` (float) - Envelope sensitivity, 0.0 to 1.0 (default: 0.5)
- `attack_ms` (float) - Envelope attack time in ms, 1.0 to 100.0 (default: 10.0)
- `release_ms` (float) - Envelope release time in ms, 10.0 to 1000.0 (default: 200.0)
- `frequency_min_hz` (float) - Minimum filter frequency in Hz, 100.0 to 2000.0 (default: 300.0)
- `frequency_max_hz` (float) - Maximum filter frequency in Hz, 500.0 to 8000.0 (default: 3000.0)
- `resonance` (float) - Filter resonance/Q, 0.5 to 10.0 (default: 2.0)
- `mix` (float) - Dry/wet mix 0.0 to 1.0 (default: 1.0)

**Modulatable:** `sensitivity`, `attack_ms`, `release_ms`, `frequency_min_hz`, `frequency_max_hz`, `resonance`, `mix`

**Implementation Details:**
- Envelope follower with peak detection and attack/release smoothing
- Envelope maps to State Variable Filter (bandpass mode) cutoff frequency
- Expressive, input-responsive filtering

---

## Effect Chaining

Effects can be chained in series or parallel using the graph connections.

**Series Example:**
```json
"connections": [
  { "from": "input:L", "to": "pitch_shift:in" },
  { "from": "pitch_shift:out", "to": "reverb:in" },
  { "from": "reverb:out", "to": "output:L" }
]
```

**Parallel Example (using mix node):**
```json
"connections": [
  { "from": "input:L", "to": "delay:in" },
  { "from": "input:L", "to": "reverb:in" },
  { "from": "delay:out", "to": "mix1:in_a" },
  { "from": "reverb:out", "to": "mix1:in_b" },
  { "from": "mix1:out", "to": "output:L" }
]
```

---

## Modulation

Any parameter marked as "Modulatable" can be controlled by LFOs or other control-rate signals.

**Steps to modulate a parameter:**

1. Declare the parameter input in `param_inputs`:
   ```json
   "param_inputs": ["decay", "mix"]
   ```

2. Create an LFO or other control source:
   ```json
   "lfo1": {
     "type": "lfo",
     "rate": "control",
     "outputs": ["value"],
     "params": { "freq_hz": 0.3, "depth": 0.2 }
   }
   ```

3. Connect the LFO to the parameter:
   ```json
   { "from": "lfo1:value", "to": "reverb:decay" }
   ```

**Modulation is additive:** The LFO output is added to the base parameter value.

---

## Test Files

Example .dpt files are available in the `graphs/` directory:

**Basic Effects:**
- `test_reverb.dpt` - Basic reverb effect
- `test_lfo_reverb.dpt` - Reverb with LFO modulation on decay
- `test_pitch_shift.dpt` - Basic pitch shift (+5 semitones)
- `test_lfo_pitch_shift.dpt` - Pitch shift with LFO modulation
- `test_reverb_pitch_shift.dpt` - Combined reverb and pitch shift with dual LFO modulation

**Priority 1 Foundation Effects:**
- `test_svfilter.dpt` - Basic lowpass filter
- `test_lfo_filter.dpt` - LFO-modulated filter sweep (cutoff and resonance)
- `test_reverse.dpt` - Basic reverse playback
- `test_reverse_morph.dpt` - LFO-modulated forward/reverse morphing
- `test_multitap_simple.dpt` - Simple 4-tap delay
- `test_multitap_rhythmic.dpt` - 8-tap rhythmic pattern
- `test_filter_reverse.dpt` - Filtered reverse effect
- `test_foundation_chain.dpt` - All three Priority 1 effects chained (filter + multitap + reverse)

**Priority 2 Granular Synthesis:**
- `granular_basic.dpt` - Basic granular texture (50ms grains, medium density)
- `granular_cloud.dpt` - Dense granular cloud with LFO modulation
- `granular_shimmer.dpt` - Pitched granular shimmer (+12 semitones with LFO)
- `granular_filtered.dpt` - Filtered granular (-7 semitones with resonant filter)
- `granular_ambient_pad.dpt` - Full ambient pad (4 LFOs modulating grain/pitch/filter/reverb)
- `granular_freeze.dpt` - Freeze effect (high density, minimal randomization)

**Chorus/Harmonizer Examples:**
- `chorus_harmonizer.dpt` - Major chord harmonization (root + major 3rd + perfect 5th)
- `chorus_harmonizer_minor.dpt` - Minor chord harmonization (root + minor 3rd + perfect 5th)
- `chorus_shimmer.dpt` - Shimmer effect with LFO detune + reverb
- `chorus_octave.dpt` - Triple octave effect (-1 octave, root, +1 octave)

**Priority 3 Looper & Glitch:**
- `looper_basic.dpt` - Basic phrase looper with record/play
- `looper_overdub.dpt` - Looper with overdubbing workflow
- `looper_speed_mod.dpt` - LFO-modulated playback speed
- `glitch_basic.dpt` - Basic freeze/stutter effect
- `glitch_modulated.dpt` - LFO-modulated stutter rate
- `glitch_randomized.dpt` - High randomization glitch
- `looper_glitch_combo.dpt` - Combined looper + glitch effects
- `microcosm_style.dpt` - Full Microcosm-style effect chain (looper + granular + glitch + reverse + filter + reverb)

**Ambient Examples:**
- `ambient_texture_1.dpt` - Filtered granular shimmer with evolving reverb
- `ambient_texture_2.dpt` - Dense granular cloud with reverse echo
- `ambient_texture_3.dpt` - Multi-octave shimmer pad with filter sweeps

**User-Friendly Effects (Modulation Trio):**
- `test_chorus.dpt` - Basic chorus effect with 3 voices
- `test_tremolo.dpt` - Tremolo with sine wave modulation
- `test_flanger.dpt` - Classic flanger with moderate feedback

**User-Friendly Effects (Dynamics & Saturation):**
- `test_compressor.dpt` - Compressor with 4:1 ratio and makeup gain
- `test_overdrive.dpt` - Soft overdrive with tone control
- `test_autowah.dpt` - Auto-wah with envelope-controlled filter sweep

Run examples:
```bash
# Basic effects
./build/dendrophone graphs/test_reverb.dpt
./build/dendrophone graphs/chorus_harmonizer.dpt

# Priority 1 effects
./build/dendrophone graphs/test_svfilter.dpt
./build/dendrophone graphs/test_reverse_morph.dpt
./build/dendrophone graphs/test_multitap_rhythmic.dpt
./build/dendrophone graphs/test_foundation_chain.dpt

# Priority 2 granular synthesis
./build/dendrophone graphs/granular_cloud.dpt
./build/dendrophone graphs/granular_shimmer.dpt
./build/dendrophone graphs/granular_ambient_pad.dpt
./build/dendrophone graphs/granular_freeze.dpt

# Priority 3 looper & glitch
./build/dendrophone graphs/looper_basic.dpt
./build/dendrophone graphs/looper_overdub.dpt
./build/dendrophone graphs/glitch_modulated.dpt
./build/dendrophone graphs/looper_glitch_combo.dpt
./build/dendrophone graphs/microcosm_style.dpt

# User-friendly effects
./build/dendrophone graphs/test_chorus.dpt
./build/dendrophone graphs/test_tremolo.dpt
./build/dendrophone graphs/test_flanger.dpt
./build/dendrophone graphs/test_compressor.dpt
./build/dendrophone graphs/test_overdrive.dpt
./build/dendrophone graphs/test_autowah.dpt
```

---

## Implementation Notes

All effects follow the same architecture:

1. **State struct** - Contains buffers, parameters, and configuration
2. **State creation** - Control-thread allocation with 64-byte alignment
3. **State destruction** - Control-thread cleanup
4. **DSP operation** - Real-time audio processing function

Parameters use the `Param` struct which provides:
- `base` - Static value from .dpt file
- `mod` - Accumulated modulation (reset each block)
- `smoothed` - Smoothed value for audio-rate use
- `smooth_coeff` - Smoothing coefficient to prevent zipper noise
