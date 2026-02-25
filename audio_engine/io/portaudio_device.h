#pragma once

#include "audio_device.h"
#include "ring_buffer.h"
#include <portaudio.h>
#include <atomic>
#include <memory>
#include <string>

class PortAudioDevice : public AudioDevice {
public:
    struct Config {
        int input_device_index  = -1;   // -1 = Pa_GetDefaultInputDevice()
        int output_device_index = -1;   // -1 = Pa_GetDefaultOutputDevice()
        int sample_rate   = 48000;
        int buffer_frames = 64;
        int channels      = 2;
    };

    explicit PortAudioDevice(const Config& config);
    ~PortAudioDevice() override;

    PortAudioDevice(const PortAudioDevice&) = delete;
    PortAudioDevice& operator=(const PortAudioDevice&) = delete;

    bool open() override;
    void close() override;

    int sample_rate()  const override { return config_.sample_rate; }
    int buffer_size()  const override { return config_.buffer_frames; }
    int channels()     const override { return config_.channels; }

    bool read(float* input, int frames) override;
    bool write(const float* output, int frames) override;

    // Print all available PortAudio devices to stderr.
    // Safe to call before constructing a PortAudioDevice instance.
    static void list_devices();

private:
    bool init_portaudio();
    void log_pa_error(const char* context, PaError err) const;

    // Callback functions invoked by CoreAudio's RT threads.
    static int input_callback(const void* inputBuffer, void* outputBuffer,
                              unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo* timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void* userData);
    static int output_callback(const void* inputBuffer, void* outputBuffer,
                               unsigned long framesPerBuffer,
                               const PaStreamCallbackTimeInfo* timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void* userData);

    Config    config_;
    PaStream* input_stream_   = nullptr;
    PaStream* output_stream_  = nullptr;
    bool      pa_initialized_ = false;
    int       actual_in_channels_  = 0;
    int       actual_out_channels_ = 0;

    std::unique_ptr<RingBuffer> input_ring_;   // input_callback -> read()
    std::unique_ptr<RingBuffer> output_ring_;  // write() -> output_callback
    std::atomic<bool> closing_{false};         // signals read() to stop waiting
};
