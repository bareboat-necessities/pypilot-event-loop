#pragma once

#include <Arduino.h>
#include "pypilot_event_loop/pin_event.hpp"

namespace pypilot_event_loop {

/**
 * Arduino digital input implementation of IPinInput.
 *
 * This wrapper is intentionally small. Pin mode setup remains explicit through
 * begin(), so callers can decide whether the pin should use INPUT or
 * INPUT_PULLUP.
 */
class ArduinoDigitalInputPin final : public IPinInput {
public:
    explicit ArduinoDigitalInputPin(uint8_t pin) : pin_(pin) {}

    void begin(uint8_t mode = INPUT) {
        pinMode(pin_, mode);
        ready_ = true;
    }

    bool valid() const override { return ready_; }

    bool read() const override {
        return digitalRead(pin_) == HIGH;
    }

    uint32_t id() const override { return pin_; }

private:
    uint8_t pin_ = 0;
    bool ready_ = false;
};

} // namespace pypilot_event_loop
