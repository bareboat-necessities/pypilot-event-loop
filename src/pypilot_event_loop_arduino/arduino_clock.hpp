#pragma once

#include <Arduino.h>
#include "pypilot_event_loop/clock.hpp"

namespace pypilot_event_loop {

class ArduinoClock final : public IClock {
public:
    uint64_t micros() const override {
        return static_cast<uint64_t>(::micros());
    }
};

} // namespace pypilot_event_loop
