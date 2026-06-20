#pragma once

#if defined(__linux__) && defined(PYPILOT_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO)
#include <gpiod.h>
#include <stddef.h>

#include "pypilot_event_loop/pin_io.hpp"

namespace pypilot_event_loop {

/**
 * Linux libgpiod digital input pin.
 *
 * This backend claims the requested GPIO line through libgpiod and configures it
 * as an input. It is intentionally opt-in so the default Linux build can still
 * use lightweight file-backed pin paths without requiring libgpiod headers.
 */
class LinuxGpiodDigitalInputPin final : public IDigitalInputPin {
public:
    LinuxGpiodDigitalInputPin(const char* chip_name,
                              unsigned int line_offset,
                              const char* consumer = "pypilot_event_loop") {
        open_input(chip_name, line_offset, consumer);
    }

    LinuxGpiodDigitalInputPin(const LinuxGpiodDigitalInputPin&) = delete;
    LinuxGpiodDigitalInputPin& operator=(const LinuxGpiodDigitalInputPin&) = delete;

    ~LinuxGpiodDigitalInputPin() override { close(); }

    bool valid() const override { return line_ != nullptr; }

    bool read() override {
        if (!line_) {
            return false;
        }
        const int value = gpiod_line_get_value(line_);
        return value > 0;
    }

private:
    void open_input(const char* chip_name, unsigned int line_offset, const char* consumer) {
        if (!chip_name || !*chip_name) {
            return;
        }
        chip_ = gpiod_chip_open_by_name(chip_name);
        if (!chip_) {
            return;
        }
        line_ = gpiod_chip_get_line(chip_, line_offset);
        if (!line_) {
            close();
            return;
        }
        if (gpiod_line_request_input(line_, consumer ? consumer : "pypilot_event_loop") != 0) {
            close();
        }
    }

    void close() {
        if (line_) {
            gpiod_line_release(line_);
            line_ = nullptr;
        }
        if (chip_) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
        }
    }

    gpiod_chip* chip_ = nullptr;
    gpiod_line* line_ = nullptr;
};

/**
 * Linux libgpiod digital output pin.
 *
 * This backend claims the requested GPIO line through libgpiod and configures it
 * as an output with the requested initial level.
 */
class LinuxGpiodDigitalOutputPin final : public IDigitalOutputPin {
public:
    LinuxGpiodDigitalOutputPin(const char* chip_name,
                               unsigned int line_offset,
                               bool initial = false,
                               const char* consumer = "pypilot_event_loop") {
        open_output(chip_name, line_offset, initial, consumer);
    }

    LinuxGpiodDigitalOutputPin(const LinuxGpiodDigitalOutputPin&) = delete;
    LinuxGpiodDigitalOutputPin& operator=(const LinuxGpiodDigitalOutputPin&) = delete;

    ~LinuxGpiodDigitalOutputPin() override { close(); }

    bool valid() const override { return line_ != nullptr; }

    bool write(bool high) override {
        if (!line_) {
            return false;
        }
        return gpiod_line_set_value(line_, high ? 1 : 0) == 0;
    }

private:
    void open_output(const char* chip_name, unsigned int line_offset, bool initial, const char* consumer) {
        if (!chip_name || !*chip_name) {
            return;
        }
        chip_ = gpiod_chip_open_by_name(chip_name);
        if (!chip_) {
            return;
        }
        line_ = gpiod_chip_get_line(chip_, line_offset);
        if (!line_) {
            close();
            return;
        }
        if (gpiod_line_request_output(line_, consumer ? consumer : "pypilot_event_loop", initial ? 1 : 0) != 0) {
            close();
        }
    }

    void close() {
        if (line_) {
            gpiod_line_release(line_);
            line_ = nullptr;
        }
        if (chip_) {
            gpiod_chip_close(chip_);
            chip_ = nullptr;
        }
    }

    gpiod_chip* chip_ = nullptr;
    gpiod_line* line_ = nullptr;
};

} // namespace pypilot_event_loop
#endif
