#pragma once

#include <Arduino.h>
#include "pypilot_event_loop/clock.hpp"
#include "pypilot_event_loop/micros32_extender.hpp"

namespace pypilot_event_loop {

class ArduinoClock final : public IClock {
public:
    uint64_t micros() const override {
        return extender_.update(static_cast<uint32_t>(::micros()));
    }

private:
    mutable Micros32Extender extender_;
};

} // namespace pypilot_event_loop
