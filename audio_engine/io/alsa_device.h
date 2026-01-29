#pragma once

#include "audio_device.h"
#include <alsa/asoundlib.h>
#include <string>
#include <vector>

class AlsaAudioDevice : public AudioDevice {
public:
    struct Config {
        std::string capture_device = "hw:2,0";
        std::string playback_device = "hw:2,0";
        int sample_rate = 48000;
        int buffer_frames = 64;
        int periods = 2;
        int channels = 2;
    };

    explicit AlsaAudioDevice(const Config& config);
    ~AlsaAudioDevice() override;

    AlsaAudioDevice(const AlsaAudioDevice&) = delete;
    AlsaAudioDevice& operator=(const AlsaAudioDevice&) = delete;

    bool open() override;
    void close() override;

    int sample_rate() const override { return config_.sample_rate; }
    int buffer_size() const override { return config_.buffer_frames; }
    int channels() const override { return config_.channels; }

    bool read(float* output, int frames) override;
    bool write(const float* input, int frames) override;

private:
    bool configure_stream(snd_pcm_t* pcm, const char* name);
    void interleave(const float* src, int16_t* dst, int frames);
    void deinterleave(const int16_t* src, float* dst, int frames);

    Config config_;
    snd_pcm_t* capture_handle_ = nullptr;
    snd_pcm_t* playback_handle_ = nullptr;

    std::vector<int16_t> capture_buffer_;
    std::vector<int16_t> playback_buffer_;
};
