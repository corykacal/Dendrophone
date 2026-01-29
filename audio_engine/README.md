# Dendrophone Audio Engine

Hard-real-time, low-latency digital audio processing engine for Raspberry Pi CM4.

## Dependencies

```bash
# Install build dependencies
sudo apt install build-essential libasound2-dev

# Optional: CMake for alternative build
sudo apt install cmake
```

## Build

```bash
cd audio_engine

# Using Make
make

# Or using CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

## Run

```bash
# Run with default 64-frame buffer
sudo ./build/dendrophone

# Run with custom buffer size (e.g., 128 frames)
sudo ./build/dendrophone 128
```

Note: `sudo` is required for real-time thread priority (SCHED_FIFO).

## Hardware

- Raspberry Pi CM4
- Creative Sound Blaster Play! 3 (USB Audio Class)
- ALSA driver (hw:2,0 by default)

## Architecture

```
.dpt file → DptParser → Graph AST → [Compiler] → DSPProgram (RT)
                                         ↓
ADC → AudioDevice::read() → AudioEngine → DSPProgram → AudioDevice::write() → DAC
```

- Audio thread: lock-free, allocation-free, deterministic
- Control thread: heap allowed, graph construction
- DSP program: precompiled, immutable during execution

## .dpt File Format

Human-readable JSON graph description:

```json
{
  "dpt_version": 1,
  "audio": { "inputs": ["L", "R"], "outputs": ["L", "R"] },
  "nodes": {
    "input": { "type": "input", "outputs": ["L", "R"] },
    "delay": { "type": "delay", "inputs": ["in"], "outputs": ["out"],
               "params": { "time_ms": 250, "feedback": 0.4, "mix": 0.5 } },
    "output": { "type": "output", "inputs": ["L", "R"] }
  },
  "connections": [
    { "from": "input:L", "to": "delay:in" },
    { "from": "delay:out", "to": "output:L" }
  ]
}
```
