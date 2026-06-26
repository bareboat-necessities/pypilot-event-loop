#pragma once

#include <stddef.h>
#include <stdint.h>

namespace async_event_loop {

/**
 * Portable datagram stream interface.
 *
 * Linux fd-backed implementations should return a non-negative native_fd() so
 * EventLoop::on_readable() can use libevent fd readiness. Arduino and in-memory
 * streams return -1 and are checked cooperatively from tick().
 */
class IDatagramStream {
public:
    virtual ~IDatagramStream() = default;

    virtual int recv(uint8_t* dst, size_t max_len) = 0;
    virtual int send(const uint8_t* src, size_t len) = 0;

    virtual bool readable() const = 0;
    virtual bool valid() const = 0;

    virtual int native_fd() const { return -1; }
};

} // namespace async_event_loop
