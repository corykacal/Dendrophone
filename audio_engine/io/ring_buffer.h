#pragma once

#include <atomic>
#include <algorithm>
#include <cstring>
#include <cstdint>

// Lock-free Single-Producer Single-Consumer ring buffer for float audio samples.
// Capacity must be a power of 2.
// Thread safety: push() called from one thread only, pop()/available() from another only.
// head_ is owned by the producer (push), tail_ is owned by the consumer (pop).
class RingBuffer {
public:
    explicit RingBuffer(int capacity)
        : buf_(new float[capacity])
        , capacity_(capacity)
        , mask_(capacity - 1)
    {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    ~RingBuffer() { delete[] buf_; }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // Push up to `count` floats from `src`. Returns samples pushed.
    // Never blocks. May push fewer than `count` if buffer is nearly full.
    int push(const float* src, int count) {
        int head = head_.load(std::memory_order_relaxed);
        int tail = tail_.load(std::memory_order_acquire);
        int free = capacity_ - (head - tail);
        int to_push = std::min(count, free);
        if (to_push <= 0) return 0;

        int start = head & mask_;
        int end   = start + to_push;
        if (end <= capacity_) {
            memcpy(buf_ + start, src, to_push * sizeof(float));
        } else {
            int first = capacity_ - start;
            memcpy(buf_ + start, src,         first              * sizeof(float));
            memcpy(buf_,         src + first, (to_push - first)  * sizeof(float));
        }
        head_.store(head + to_push, std::memory_order_release);
        return to_push;
    }

    // Pop up to `count` floats into `dst`. Returns samples popped.
    // Never blocks. May pop fewer than `count` if buffer has less data.
    int pop(float* dst, int count) {
        int tail = tail_.load(std::memory_order_relaxed);
        int head = head_.load(std::memory_order_acquire);
        int avail = head - tail;
        int to_pop = std::min(count, avail);
        if (to_pop <= 0) return 0;

        int start = tail & mask_;
        int end   = start + to_pop;
        if (end <= capacity_) {
            memcpy(dst, buf_ + start, to_pop * sizeof(float));
        } else {
            int first = capacity_ - start;
            memcpy(dst,         buf_ + start, first             * sizeof(float));
            memcpy(dst + first, buf_,         (to_pop - first)  * sizeof(float));
        }
        tail_.store(tail + to_pop, std::memory_order_release);
        return to_pop;
    }

    // Samples available to pop. Call from consumer thread.
    int available() const {
        return head_.load(std::memory_order_acquire)
             - tail_.load(std::memory_order_relaxed);
    }

    // Reset to empty. NOT thread-safe — call only when both threads are quiescent.
    void reset() {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    static int next_power_of_2(int n) {
        if (n <= 1) return 1;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        return n + 1;
    }

private:
    float* buf_;
    const int capacity_;
    const int mask_;
    alignas(64) std::atomic<int> head_{0};  // producer advances
    alignas(64) std::atomic<int> tail_{0};  // consumer advances
};
