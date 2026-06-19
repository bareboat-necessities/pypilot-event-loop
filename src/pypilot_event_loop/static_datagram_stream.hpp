#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "datagram_stream.hpp"

namespace pypilot_event_loop {

/**
 * Small fixed-capacity datagram stream.
 *
 * Each send() stores one datagram up to PacketSize bytes. Each recv() returns
 * one datagram. This mirrors packet-oriented transports while staying portable
 * enough for Linux and Arduino examples.
 */
template<size_t PacketCapacity, size_t PacketSize>
class StaticDatagramStream final : public IDatagramStream {
public:
    int recv(uint8_t* dst, size_t max_len) override {
        if (!dst || max_len == 0 || !readable()) {
            return 0;
        }
        Packet& packet = packets_[head_];
        const size_t n = packet.size < max_len ? packet.size : max_len;
        memcpy(dst, packet.data, n);
        packet.size = 0;
        head_ = (head_ + 1) % PacketCapacity;
        --count_;
        return static_cast<int>(n);
    }

    int send(const uint8_t* src, size_t len) override {
        if (!src || len == 0 || count_ >= PacketCapacity) {
            return 0;
        }
        Packet& packet = packets_[tail_];
        const size_t n = len < PacketSize ? len : PacketSize;
        memcpy(packet.data, src, n);
        packet.size = n;
        tail_ = (tail_ + 1) % PacketCapacity;
        ++count_;
        return static_cast<int>(n);
    }

    bool readable() const override { return count_ > 0; }
    bool valid() const override { return true; }
    size_t size() const { return count_; }
    size_t capacity() const { return PacketCapacity; }

private:
    struct Packet {
        uint8_t data[PacketSize]{};
        size_t size = 0;
    };

    Packet packets_[PacketCapacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

} // namespace pypilot_event_loop
