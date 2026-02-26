#include "portaudio_device.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

PortAudioDevice::PortAudioDevice(const Config& config)
    : config_(config) {}

PortAudioDevice::~PortAudioDevice() {
    close();
    if (pa_initialized_) {
        Pa_Terminate();
        pa_initialized_ = false;
    }
}

// ---------- static helper ----------

void PortAudioDevice::list_devices() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio: Pa_Initialize failed: %s\n", Pa_GetErrorText(err));
        return;
    }

    int count = Pa_GetDeviceCount();
    if (count < 0) {
        fprintf(stderr, "PortAudio: Pa_GetDeviceCount error: %s\n", Pa_GetErrorText(count));
        Pa_Terminate();
        return;
    }

    PaDeviceIndex default_in  = Pa_GetDefaultInputDevice();
    PaDeviceIndex default_out = Pa_GetDefaultOutputDevice();

    fprintf(stderr, "\nAvailable PortAudio devices (%d total):\n", count);
    fprintf(stderr, "  %-4s  %-40s  %-6s  %-6s  %s\n",
            "IDX", "NAME", "IN_CH", "OUT_CH", "HOST API");

    for (int i = 0; i < count; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;
        const PaHostApiInfo* api = Pa_GetHostApiInfo(info->hostApi);
        const char* api_name = api ? api->name : "?";

        fprintf(stderr, "  %-4d  %-40s  %-6d  %-6d  %s%s%s\n",
                i,
                info->name,
                info->maxInputChannels,
                info->maxOutputChannels,
                api_name,
                (i == default_in)  ? " [default-in]"  : "",
                (i == default_out) ? " [default-out]" : "");
    }
    fprintf(stderr, "\n");

    Pa_Terminate();
}

// ---------- private helpers ----------

bool PortAudioDevice::init_portaudio() {
    if (pa_initialized_) return true;
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio: Pa_Initialize failed: %s\n", Pa_GetErrorText(err));
        return false;
    }
    pa_initialized_ = true;
    return true;
}

void PortAudioDevice::log_pa_error(const char* context, PaError err) const {
    fprintf(stderr, "PortAudio %s: %s\n", context, Pa_GetErrorText(err));
}

// ---------- open ----------

bool PortAudioDevice::open() {
    if (!init_portaudio()) return false;

    PaDeviceIndex in_idx  = (config_.input_device_index  < 0)
                            ? Pa_GetDefaultInputDevice()
                            : (PaDeviceIndex)config_.input_device_index;
    PaDeviceIndex out_idx = (config_.output_device_index < 0)
                            ? Pa_GetDefaultOutputDevice()
                            : (PaDeviceIndex)config_.output_device_index;

    if (in_idx == paNoDevice) {
        fprintf(stderr, "PortAudio: no input device available\n");
        list_devices();
        return false;
    }
    if (out_idx == paNoDevice) {
        fprintf(stderr, "PortAudio: no output device available\n");
        list_devices();
        return false;
    }

    // If output was not explicitly specified and the default output is the same
    // device as the input (e.g. BlackHole set as system default output), we
    // cannot guess which real output the user wants — list all candidates and
    // ask them to pick one explicitly.
    if (config_.output_device_index < 0 && out_idx == in_idx) {
        const PaDeviceInfo* in_info_tmp = Pa_GetDeviceInfo(in_idx);
        fprintf(stderr, "\nError: no output device specified, and the system default output\n");
        fprintf(stderr, "       ('%s') is the same as the input device.\n\n",
                in_info_tmp ? in_info_tmp->name : "?");

        int count = Pa_GetDeviceCount();
        fprintf(stderr, "Available output devices:\n");
        fprintf(stderr, "  %-4s  %-44s  %s\n", "IDX", "NAME", "CH");
        for (int i = 0; i < count; i++) {
            if (i == in_idx) continue;
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (!info || info->maxOutputChannels < 1) continue;
            fprintf(stderr, "  %-4d  %-44s  %d\n",
                    i, info->name, info->maxOutputChannels);
        }

        fprintf(stderr, "\nRe-run with one of:\n");
        fprintf(stderr, "  --output-name \"MacBook\"        (partial name match)\n");
        fprintf(stderr, "  --output-name \"Headphones\"\n");
        fprintf(stderr, "  --output-name \"USB\"\n");
        fprintf(stderr, "  --output-device <IDX>          (exact index from list above)\n\n");
        return false;
    }

    const PaDeviceInfo* in_info  = Pa_GetDeviceInfo(in_idx);
    const PaDeviceInfo* out_info = Pa_GetDeviceInfo(out_idx);

    double suggested_latency = (double)config_.buffer_frames / config_.sample_rate;

    // Clamp channel count to what the device actually supports
    actual_in_channels_  = in_info
        ? std::min(config_.channels, in_info->maxInputChannels)
        : config_.channels;
    actual_out_channels_ = out_info
        ? std::min(config_.channels, out_info->maxOutputChannels)
        : config_.channels;

    if (actual_in_channels_ < config_.channels)
        fprintf(stderr, "PortAudio: input device has %d ch, requested %d — will upmix\n",
                actual_in_channels_, config_.channels);
    if (actual_out_channels_ < config_.channels)
        fprintf(stderr, "PortAudio: output device has %d ch, requested %d — will downmix\n",
                actual_out_channels_, config_.channels);

    // Use the device's default low-latency if it's already lower than our buffer
    double in_latency  = suggested_latency;
    double out_latency = suggested_latency;
    if (in_info  && in_info->defaultLowInputLatency   < in_latency)
        in_latency  = in_info->defaultLowInputLatency;
    if (out_info && out_info->defaultLowOutputLatency < out_latency)
        out_latency = out_info->defaultLowOutputLatency;

    PaStreamParameters in_params;
    memset(&in_params, 0, sizeof(in_params));
    in_params.device                    = in_idx;
    in_params.channelCount              = actual_in_channels_;
    in_params.sampleFormat              = paFloat32;
    in_params.suggestedLatency          = in_latency;
    in_params.hostApiSpecificStreamInfo = nullptr;

    PaStreamParameters out_params;
    memset(&out_params, 0, sizeof(out_params));
    out_params.device                    = out_idx;
    out_params.channelCount              = actual_out_channels_;
    out_params.sampleFormat              = paFloat32;
    out_params.suggestedLatency          = out_latency;
    out_params.hostApiSpecificStreamInfo = nullptr;

    // Allocate ring buffers now that buffer size and channels are known.
    // Capacity: at least 4096 samples, or 32x one buffer — ~42ms at 48kHz/64fr/2ch.
    int ring_cap = RingBuffer::next_power_of_2(
        std::max(4096, config_.buffer_frames * config_.channels * 32));
    input_ring_  = std::make_unique<RingBuffer>(ring_cap);
    output_ring_ = std::make_unique<RingBuffer>(ring_cap);
    closing_.store(false, std::memory_order_relaxed);

    // Open input stream (callback mode — CoreAudio RT thread drives input)
    PaError err = Pa_OpenStream(
        &input_stream_,
        &in_params,
        nullptr,
        config_.sample_rate,
        config_.buffer_frames,
        paClipOff,
        &PortAudioDevice::input_callback,
        static_cast<void*>(this));
    if (err != paNoError) {
        log_pa_error("open input", err);
        fprintf(stderr, "PortAudio: requested device index %d\n", in_idx);
        list_devices();
        return false;
    }

    // Open output stream (callback mode — CoreAudio RT thread drives output)
    err = Pa_OpenStream(
        &output_stream_,
        nullptr,
        &out_params,
        config_.sample_rate,
        config_.buffer_frames,
        paClipOff,
        &PortAudioDevice::output_callback,
        static_cast<void*>(this));
    if (err != paNoError) {
        log_pa_error("open output", err);
        Pa_CloseStream(input_stream_);
        input_stream_ = nullptr;
        list_devices();
        return false;
    }

    err = Pa_StartStream(input_stream_);
    if (err != paNoError) {
        log_pa_error("start input", err);
        Pa_CloseStream(input_stream_);
        Pa_CloseStream(output_stream_);
        input_stream_ = output_stream_ = nullptr;
        return false;
    }

    err = Pa_StartStream(output_stream_);
    if (err != paNoError) {
        log_pa_error("start output", err);
        Pa_StopStream(input_stream_);
        Pa_CloseStream(input_stream_);
        Pa_CloseStream(output_stream_);
        input_stream_ = output_stream_ = nullptr;
        return false;
    }

    // Pre-fill output ring with one buffer of silence so the output callback
    // has data from its very first invocation.
    {
        float silence[1024 * 2] = {};
        output_ring_->push(silence, config_.buffer_frames * actual_out_channels_);
    }

    const PaStreamInfo* in_si  = Pa_GetStreamInfo(input_stream_);
    const PaStreamInfo* out_si = Pa_GetStreamInfo(output_stream_);

    fprintf(stderr, "PortAudio: opened device #%d (%s) for input\n",
            in_idx, in_info ? in_info->name : "?");
    fprintf(stderr, "PortAudio: opened device #%d (%s) for output\n",
            out_idx, out_info ? out_info->name : "?");
    fprintf(stderr, "PortAudio: %d Hz, %d frames/buffer, %d channels\n",
            config_.sample_rate, config_.buffer_frames, config_.channels);
    if (in_si)
        fprintf(stderr, "PortAudio: input latency  = %.2f ms\n", in_si->inputLatency  * 1000.0);
    if (out_si)
        fprintf(stderr, "PortAudio: output latency = %.2f ms\n", out_si->outputLatency * 1000.0);

    return true;
}

// ---------- close ----------

void PortAudioDevice::close() {
    // Signal read()'s spin-wait to unblock before stopping streams.
    closing_.store(true, std::memory_order_release);

    if (input_stream_) {
        Pa_StopStream(input_stream_);
        Pa_CloseStream(input_stream_);
        input_stream_ = nullptr;
    }
    if (output_stream_) {
        Pa_StopStream(output_stream_);
        Pa_CloseStream(output_stream_);
        output_stream_ = nullptr;
    }

    if (input_ring_)  input_ring_->reset();
    if (output_ring_) output_ring_->reset();
}

// ---------- callbacks (called by CoreAudio RT threads) ----------

int PortAudioDevice::input_callback(const void* inputBuffer, void* /*outputBuffer*/,
                                    unsigned long framesPerBuffer,
                                    const PaStreamCallbackTimeInfo* /*timeInfo*/,
                                    PaStreamCallbackFlags /*statusFlags*/,
                                    void* userData) {
    auto* self = static_cast<PortAudioDevice*>(userData);
    if (inputBuffer) {
        self->input_ring_->push(static_cast<const float*>(inputBuffer),
                                static_cast<int>(framesPerBuffer) * self->actual_in_channels_);
    }
    // If inputBuffer == nullptr, a glitch occurred — push nothing; read() will spin briefly.
    return paContinue;
}

int PortAudioDevice::output_callback(const void* /*inputBuffer*/, void* outputBuffer,
                                     unsigned long framesPerBuffer,
                                     const PaStreamCallbackTimeInfo* /*timeInfo*/,
                                     PaStreamCallbackFlags /*statusFlags*/,
                                     void* userData) {
    auto* self = static_cast<PortAudioDevice*>(userData);
    float* out = static_cast<float*>(outputBuffer);
    int needed = static_cast<int>(framesPerBuffer) * self->actual_out_channels_;
    int got = self->output_ring_->pop(out, needed);
    if (got < needed)
        memset(out + got, 0, static_cast<size_t>(needed - got) * sizeof(float));
    return paContinue;
}

// ---------- read / write ----------

bool PortAudioDevice::read(float* input, int frames) {
    const int needed = frames * actual_in_channels_;
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(100);

    // Yield until the input callback has filled a full buffer's worth.
    while (input_ring_->available() < needed) {
        if (closing_.load(std::memory_order_acquire)) return false;
        if (std::chrono::steady_clock::now() > deadline)   return false;
        std::this_thread::yield();
    }

    if (actual_in_channels_ == config_.channels) {
        // Fast path: channel counts match, pop directly into caller's buffer.
        input_ring_->pop(input, frames * config_.channels);
    } else {
        // Upmix: pop device-native channels, then expand to config_.channels.
        float raw[4096];
        input_ring_->pop(raw, needed);
        for (int i = 0; i < frames; i++)
            for (int c = 0; c < config_.channels; c++)
                input[i * config_.channels + c] =
                    raw[i * actual_in_channels_ + (c < actual_in_channels_ ? c : 0)];
    }
    return true;
}

bool PortAudioDevice::write(const float* output, int frames) {
    if (actual_out_channels_ == config_.channels) {
        // Fast path: channel counts match, push directly.
        output_ring_->push(output, frames * config_.channels);
    } else {
        // Downmix: average all channels to mono.
        float mono[4096];
        for (int i = 0; i < frames; i++) {
            float sum = 0.0f;
            for (int c = 0; c < config_.channels; c++)
                sum += output[i * config_.channels + c];
            mono[i] = sum / config_.channels;
        }
        output_ring_->push(mono, frames * actual_out_channels_);
    }
    return true;
}
