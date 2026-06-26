#pragma once

#include <stdint.h>

namespace async_event_loop {

class IClock {
public:
    virtual ~IClock() = default;
    virtual uint64_t micros() const = 0;
};

} // namespace async_event_loop
