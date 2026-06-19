#pragma once

#include <stddef.h>
#include <stdint.h>

#include "byte_stream.hpp"

namespace pypilot_event_loop {

/**
 * Small fixed-capacity byte stream.
 *
 * This is useful for tests, examples, and small in-memory adapters. Production
 * Linux serial/TCP streams and Arduino Serial streams still implement the same
 * IByteStream interface, so code written against IByteStream remains portable.
 */
template<size_t Capacity>
class StaticByteStream final : public IByteStream {
public:
    int read(uint8_t* dst, size_t max_len) override {
        if (!dst || max_len == 0) {
            return 0;
        }
        size_t n = 0;
        while (n < max_len && readable()) {
            dst[n++] = buffer_[head_];
            head_ = (head_ + 1) % Capacity;
            --count_;
        }
        return static_cast<int>(n);
    }

    int write(const uint8_t* src, size_t len) override {
        if (!src || len == 0) {
            return 0;
        }
        size_t n = 0;
        while (n < len && count_ < Capacity) {
            buffer_[tail_] = src[n++];
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
        }
        return static_cast<int>(n);
    }

    bool readable() const override { return count_ > 0; }
    bool writable() const override { return count_ < Capacity; }
    bool valid() const override { return true; }
    size_t size() const { return count_; }
    size_t capacity() const { return Capacity; }

private:
    uint8_t buffer_[Capacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

} // namespace pypilot_event_loop
