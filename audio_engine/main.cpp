#include "io/alsa_device.h"
#include "engine/audio_engine.h"
#include "core/dsp_program_builder.h"
#include "core/dsp_ops.h"
#include <cstdio>
#include <csignal>
#include <atomic>
#include <unistd.h>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false, std::memory_order_release);
}

int main(int argc, char* argv[]) {
    fprintf(stderr, "Dendrophone Audio Engine - Phase 3 Test\n");
    fprintf(stderr, "========================================\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    AlsaAudioDevice::Config config;
    config.capture_device = "hw:0,0";
    config.playback_device = "hw:0,0";
    config.sample_rate = 48000;
    config.buffer_frames = 64;
    config.periods = 2;
    config.channels = 2;

    if (argc > 1) {
        config.buffer_frames = atoi(argv[1]);
        fprintf(stderr, "Using buffer size: %d frames\n", config.buffer_frames);
    }

    AlsaAudioDevice device(config);

    if (!device.open()) {
        fprintf(stderr, "Failed to open audio device\n");
        return 1;
    }

    // Build DSP program: ADC → Copy → DAC
    // Buffer layout:
    //   0 = input buffer (from ADC)
    //   1 = output buffer (to DAC)
    const uint32_t buffer_samples = config.buffer_frames * config.channels;
    DSPProgramBuilder builder(buffer_samples, 2);  // 2 buffers
    builder.set_input_buffer(0);
    builder.set_output_buffer(1);

    // Single copy operation: input -> output
    builder.add_block(copy_op, 0, 0, 1, nullptr);

    DSPProgram* program = builder.build();
    if (!program) {
        fprintf(stderr, "Failed to build DSP program\n");
        device.close();
        return 1;
    }
    fprintf(stderr, "DSP program built: %u blocks, %u buffers\n",
            program->num_blocks, program->num_buffers);

    AudioEngine engine(&device);
    engine.set_program(program);

    if (!engine.start()) {
        fprintf(stderr, "Failed to start engine\n");
        DSPProgramBuilder::destroy(program);
        device.close();
        return 1;
    }

    fprintf(stderr, "\nPassthrough via DSP program active. Press Ctrl+C to stop.\n");
    fprintf(stderr, "Latency: %.2f ms (%.2f ms round-trip)\n",
            (float)config.buffer_frames / config.sample_rate * 1000.0f,
            (float)config.buffer_frames / config.sample_rate * 1000.0f * 2.0f);

    while (g_running.load(std::memory_order_acquire)) {
        usleep(100000);
    }

    fprintf(stderr, "\nStopping...\n");

    engine.stop();
    DSPProgramBuilder::destroy(program);
    device.close();

    fprintf(stderr, "Done.\n");
    return 0;
}
