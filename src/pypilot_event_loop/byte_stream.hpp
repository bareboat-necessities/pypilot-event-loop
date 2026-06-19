#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pypilot_event_loop {

class IByteStream {
public:
    virtual ~IByteStream() = default;

    virtual int read(uint8_t* dst, size_t max_len) = 0;
    virtual int write(const uint8_t* src, size_t len) = 0;

    virtual bool readable() const = 0;
    virtual bool writable() const = 0;
    virtual bool valid() const = 0;
};

} // namespace pypilot_event_loop
