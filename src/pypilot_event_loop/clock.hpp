#pragma once

#include <stdint.h>

namespace pypilot_event_loop {

class IClock {
public:
    virtual ~IClock() = default;
    virtual uint64_t micros() const = 0;
};

} // namespace pypilot_event_loop
