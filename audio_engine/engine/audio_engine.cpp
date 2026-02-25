#include "audio_engine.h"
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#ifdef __linux__
#include <sched.h>
#endif

static constexpr int MAX_FRAMES = 1024;

AudioEngine::AudioEngine(AudioDevice* device)
    : device_(device) {
}

AudioEngine::~AudioEngine() {
    stop();
}

void AudioEngine::set_program(DSPProgram* program) {
    active_program_.store(program, std::memory_order_release);
}

bool AudioEngine::start() {
    if (running_.load(std::memory_order_acquire)) {
        return false;
    }

    running_.store(true, std::memory_order_release);
    thread_ = std::thread(&AudioEngine::audio_thread, this);

    return true;
}

void AudioEngine::stop() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    running_.store(false, std::memory_order_release);

    if (thread_.joinable()) {
        thread_.join();
    }
}

void AudioEngine::set_realtime_priority() {
#ifdef __linux__
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);

    int result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (result != 0) {
        fprintf(stderr, "AudioEngine: Failed to set RT priority (run as root or set capabilities)\n");
    } else {
        fprintf(stderr, "AudioEngine: RT priority set (SCHED_FIFO, priority %d)\n",
                param.sched_priority);
    }
#else
    fprintf(stderr, "AudioEngine: RT scheduling skipped (macOS dev mode)\n");
#endif
}

void AudioEngine::pin_to_core(int core) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        fprintf(stderr, "AudioEngine: Failed to pin to core %d\n", core);
    } else {
        fprintf(stderr, "AudioEngine: Pinned to core %d\n", core);
    }
#else
    (void)core;
    fprintf(stderr, "AudioEngine: CPU affinity skipped (macOS dev mode)\n");
#endif
}

void AudioEngine::audio_thread() {
    set_realtime_priority();
    pin_to_core(3);  // Pin to core 3 (last core on CM4, isolated from system tasks)

    const int frames = device_->buffer_size();
    const int channels = device_->channels();
    const int buffer_samples = frames * channels;

    float input[MAX_FRAMES * 2];
    float output[MAX_FRAMES * 2];

    fprintf(stderr, "AudioEngine: Thread started, %d frames, %d channels\n",
            frames, channels);

    while (running_.load(std::memory_order_acquire)) {
        if (!device_->read(input, frames)) {
            continue;
        }

        DSPProgram* p = active_program_.load(std::memory_order_acquire);
        if (p) {
            // Pass frames (not total samples) - process() handles interleaving
            p->process(input, output, frames);
        } else {
            memcpy(output, input, buffer_samples * sizeof(float));
        }

        device_->write(output, frames);
    }

    fprintf(stderr, "AudioEngine: Thread stopped\n");
}
