#pragma once

#include "../io/audio_device.h"
#include "../core/dsp_program.h"
#include <atomic>
#include <thread>

class AudioEngine {
public:
    explicit AudioEngine(AudioDevice* device);
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    void set_program(DSPProgram* program);
    bool start();
    void stop();

    bool is_running() const { return running_.load(std::memory_order_acquire); }

private:
    void audio_thread();
    void set_realtime_priority();
    void pin_to_core(int core);

    AudioDevice* device_;
    std::atomic<DSPProgram*> active_program_{nullptr};
    std::atomic<bool> running_{false};
    std::thread thread_;
};
