#pragma once

#include <cstdint>

struct DSPBlock {
    void (*fn)(DSPBlock&, float*, int);
    uint32_t in_a;
    uint32_t in_b;
    uint32_t out;
    void* state;
};
