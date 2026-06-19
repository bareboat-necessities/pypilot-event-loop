#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pypilot_event_loop {

class IDatagramStream {
public:
    virtual ~IDatagramStream() = default;

    virtual int recv(uint8_t* dst, size_t max_len) = 0;
    virtual int send(const uint8_t* src, size_t len) = 0;

    virtual bool readable() const = 0;
    virtual bool valid() const = 0;
};

} // namespace pypilot_event_loop
