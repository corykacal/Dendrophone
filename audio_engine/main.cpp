#include "io/alsa_device.h"
#include "engine/audio_engine.h"
#include <cstdio>
#include <csignal>
#include <atomic>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false, std::memory_order_release);
}

int main(int argc, char* argv[]) {
    fprintf(stderr, "Dendrophone Audio Engine - Phase 1 Test\n");
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

    AudioEngine engine(&device);

    // No DSP program - engine will passthrough directly
    engine.set_program(nullptr);

    if (!engine.start()) {
        fprintf(stderr, "Failed to start engine\n");
        device.close();
        return 1;
    }

    fprintf(stderr, "\nPassthrough active. Press Ctrl+C to stop.\n");
    fprintf(stderr, "Latency: %.2f ms (%.2f ms round-trip)\n",
            (float)config.buffer_frames / config.sample_rate * 1000.0f,
            (float)config.buffer_frames / config.sample_rate * 1000.0f * 2.0f);

    while (g_running.load(std::memory_order_acquire)) {
        usleep(100000);
    }

    fprintf(stderr, "\nStopping...\n");

    engine.stop();
    device.close();

    fprintf(stderr, "Done.\n");
    return 0;
}
