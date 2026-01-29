#pragma once

#include <cstdint>

class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    virtual bool open() = 0;
    virtual void close() = 0;

    virtual int sample_rate() const = 0;
    virtual int buffer_size() const = 0;
    virtual int channels() const = 0;

    virtual bool read(float* input, int frames) = 0;
    virtual bool write(const float* output, int frames) = 0;
};
