#include "alsa_device.h"
#include <cmath>
#include <cstdio>

AlsaAudioDevice::AlsaAudioDevice(const Config& config)
    : config_(config) {
}

AlsaAudioDevice::~AlsaAudioDevice() {
    close();
}

bool AlsaAudioDevice::open() {
    int err;

    err = snd_pcm_open(&capture_handle_, config_.capture_device.c_str(),
                       SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "ALSA: Cannot open capture device %s: %s\n",
                config_.capture_device.c_str(), snd_strerror(err));
        return false;
    }

    err = snd_pcm_open(&playback_handle_, config_.playback_device.c_str(),
                       SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "ALSA: Cannot open playback device %s: %s\n",
                config_.playback_device.c_str(), snd_strerror(err));
        snd_pcm_close(capture_handle_);
        capture_handle_ = nullptr;
        return false;
    }

    if (!configure_stream(capture_handle_, "capture")) {
        close();
        return false;
    }

    if (!configure_stream(playback_handle_, "playback")) {
        close();
        return false;
    }

    capture_buffer_.resize(config_.buffer_frames * config_.channels);
    playback_buffer_.resize(config_.buffer_frames * config_.channels);

    fprintf(stderr, "ALSA: Opened %s (capture) and %s (playback)\n",
            config_.capture_device.c_str(), config_.playback_device.c_str());
    fprintf(stderr, "ALSA: %d Hz, %d frames, %d channels\n",
            config_.sample_rate, config_.buffer_frames, config_.channels);

    return true;
}

bool AlsaAudioDevice::configure_stream(snd_pcm_t* pcm, const char* name) {
    int err;
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_sw_params_t* sw_params;

    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_sw_params_alloca(&sw_params);

    err = snd_pcm_hw_params_any(pcm, hw_params);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot get hw params: %s\n", name, snd_strerror(err));
        return false;
    }

    err = snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot set access: %s\n", name, snd_strerror(err));
        return false;
    }

    err = snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot set format: %s\n", name, snd_strerror(err));
        return false;
    }

    unsigned int rate = config_.sample_rate;
    err = snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, nullptr);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot set rate: %s\n", name, snd_strerror(err));
        return false;
    }
    if (rate != (unsigned int)config_.sample_rate) {
        fprintf(stderr, "ALSA %s: Rate %d not available, using %u\n",
                name, config_.sample_rate, rate);
    }

    err = snd_pcm_hw_params_set_channels(pcm, hw_params, config_.channels);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot set channels: %s\n", name, snd_strerror(err));
        return false;
    }

    snd_pcm_uframes_t buffer_size = config_.buffer_frames * config_.periods;
    err = snd_pcm_hw_params_set_buffer_size_near(pcm, hw_params, &buffer_size);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot set buffer size: %s\n", name, snd_strerror(err));
        return false;
    }

    snd_pcm_uframes_t period_size = config_.buffer_frames;
    err = snd_pcm_hw_params_set_period_size_near(pcm, hw_params, &period_size, nullptr);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot set period size: %s\n", name, snd_strerror(err));
        return false;
    }

    err = snd_pcm_hw_params(pcm, hw_params);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot apply hw params: %s\n", name, snd_strerror(err));
        return false;
    }

    snd_pcm_uframes_t actual_buffer, actual_period;
    snd_pcm_hw_params_get_buffer_size(hw_params, &actual_buffer);
    snd_pcm_hw_params_get_period_size(hw_params, &actual_period, nullptr);
    fprintf(stderr, "ALSA %s: buffer=%lu period=%lu frames\n",
            name, actual_buffer, actual_period);

    err = snd_pcm_sw_params_current(pcm, sw_params);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot get sw params: %s\n", name, snd_strerror(err));
        return false;
    }

    err = snd_pcm_sw_params_set_start_threshold(pcm, sw_params, actual_period);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot set start threshold: %s\n", name, snd_strerror(err));
        return false;
    }

    err = snd_pcm_sw_params(pcm, sw_params);
    if (err < 0) {
        fprintf(stderr, "ALSA %s: Cannot apply sw params: %s\n", name, snd_strerror(err));
        return false;
    }

    return true;
}

void AlsaAudioDevice::close() {
    if (capture_handle_) {
        snd_pcm_drop(capture_handle_);
        snd_pcm_close(capture_handle_);
        capture_handle_ = nullptr;
    }
    if (playback_handle_) {
        snd_pcm_drain(playback_handle_);
        snd_pcm_close(playback_handle_);
        playback_handle_ = nullptr;
    }
}

bool AlsaAudioDevice::read(float* output, int frames) {
    snd_pcm_sframes_t n = snd_pcm_readi(capture_handle_, capture_buffer_.data(), frames);

    if (n < 0) {
        fprintf(stderr, "ALSA capture: %s, recovering...\n", snd_strerror(n));
        n = snd_pcm_recover(capture_handle_, n, 0);
        if (n < 0) {
            fprintf(stderr, "ALSA capture recovery failed: %s\n", snd_strerror(n));
            return false;
        }
        n = snd_pcm_readi(capture_handle_, capture_buffer_.data(), frames);
        if (n < 0) {
            return false;
        }
    }

    deinterleave(capture_buffer_.data(), output, n);
    return true;
}

bool AlsaAudioDevice::write(const float* input, int frames) {
    interleave(input, playback_buffer_.data(), frames);

    snd_pcm_sframes_t n = snd_pcm_writei(playback_handle_, playback_buffer_.data(), frames);

    if (n < 0) {
        fprintf(stderr, "ALSA playback: %s, recovering...\n", snd_strerror(n));
        n = snd_pcm_recover(playback_handle_, n, 0);
        if (n < 0) {
            fprintf(stderr, "ALSA playback recovery failed: %s\n", snd_strerror(n));
            return false;
        }
        n = snd_pcm_writei(playback_handle_, playback_buffer_.data(), frames);
        if (n < 0) {
            return false;
        }
    }

    return true;
}

void AlsaAudioDevice::interleave(const float* src, int16_t* dst, int frames) {
    const int ch = config_.channels;
    for (int i = 0; i < frames * ch; i++) {
        float sample = src[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        dst[i] = static_cast<int16_t>(sample * 32767.0f);
    }
}

void AlsaAudioDevice::deinterleave(const int16_t* src, float* dst, int frames) {
    const int ch = config_.channels;
    constexpr float scale = 1.0f / 32768.0f;
    for (int i = 0; i < frames * ch; i++) {
        dst[i] = static_cast<float>(src[i]) * scale;
    }
}
