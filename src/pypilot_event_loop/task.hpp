#pragma once

#include <stdint.h>

namespace pypilot_event_loop {

class IRuntimeTask {
public:
    virtual ~IRuntimeTask() = default;
    virtual void poll(uint64_t now_us) = 0;
};

} // namespace pypilot_event_loop
