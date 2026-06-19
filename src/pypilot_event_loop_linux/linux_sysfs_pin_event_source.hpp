#pragma once

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pypilot_event_loop/pin_event.hpp"

namespace pypilot_event_loop {

/**
 * Legacy Linux GPIO sysfs edge-event source.
 *
 * This fallback assumes /sys/class/gpio/gpioN has already been exported and the
 * current process has permission to read gpioN/value and write gpioN/edge. Use
 * LinuxGpiodPinEventSource for new deployments when libgpiod and the GPIO
 * character-device API are available.
 */
class LinuxSysfsPinEventSource final : public IPinEventSource {
public:
    LinuxSysfsPinEventSource(unsigned int gpio_number,
                             PinEventType event_type = PinEventType::Change,
                             const char* sysfs_root = "/sys/class/gpio")
        : gpio_number_(gpio_number), event_type_(event_type) {
        open(sysfs_root);
    }

    LinuxSysfsPinEventSource(const LinuxSysfsPinEventSource&) = delete;
    LinuxSysfsPinEventSource& operator=(const LinuxSysfsPinEventSource&) = delete;

    ~LinuxSysfsPinEventSource() override {
        if (value_fd_ >= 0) {
            close(value_fd_);
        }
    }

    bool valid() const override { return value_fd_ >= 0; }
    int native_fd() const override { return value_fd_; }

    bool read_event(PinEvent& event) override {
        if (value_fd_ < 0) {
            return false;
        }

        const int level = read_level();
        if (level < 0) {
            return false;
        }

        const bool current = level != 0;
        if (!has_last_) {
            last_level_ = current;
            has_last_ = true;
            return false;
        }

        const bool previous = last_level_;
        last_level_ = current;
        if (!matches(previous, current)) {
            return false;
        }

        event.type = edge_type(previous, current);
        event.timestamp_us = 0;
        event.source_id = gpio_number_;
        event.sequence = ++sequence_;
        event.level = current;
        return true;
    }

private:
    static const char* edge_name(PinEventType type) {
        switch (type) {
        case PinEventType::RisingEdge:
            return "rising";
        case PinEventType::FallingEdge:
            return "falling";
        case PinEventType::Change:
            return "both";
        case PinEventType::HighLevel:
        case PinEventType::LowLevel:
            return "both";
        }
        return "both";
    }

    void open(const char* root) {
        char edge_path[160];
        char value_path[160];
        snprintf(edge_path, sizeof(edge_path), "%s/gpio%u/edge", root, gpio_number_);
        snprintf(value_path, sizeof(value_path), "%s/gpio%u/value", root, gpio_number_);

        int edge_fd = ::open(edge_path, O_WRONLY | O_CLOEXEC);
        if (edge_fd >= 0) {
            const char* edge = edge_name(event_type_);
            (void)::write(edge_fd, edge, strlen(edge));
            close(edge_fd);
        }

        value_fd_ = ::open(value_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (value_fd_ >= 0) {
            (void)read_level();
        }
    }

    int read_level() const {
        char value = '0';
        if (lseek(value_fd_, 0, SEEK_SET) < 0) {
            return -1;
        }
        if (::read(value_fd_, &value, 1) != 1) {
            return -1;
        }
        return value == '0' ? 0 : 1;
    }

    bool matches(bool previous, bool current) const {
        switch (event_type_) {
        case PinEventType::RisingEdge:
            return !previous && current;
        case PinEventType::FallingEdge:
            return previous && !current;
        case PinEventType::Change:
            return previous != current;
        case PinEventType::HighLevel:
            return current;
        case PinEventType::LowLevel:
            return !current;
        }
        return false;
    }

    PinEventType edge_type(bool previous, bool current) const {
        if (!previous && current) {
            return PinEventType::RisingEdge;
        }
        if (previous && !current) {
            return PinEventType::FallingEdge;
        }
        return event_type_;
    }

    unsigned int gpio_number_ = 0;
    PinEventType event_type_ = PinEventType::Change;
    int value_fd_ = -1;
    uint32_t sequence_ = 0;
    bool has_last_ = false;
    bool last_level_ = false;
};

} // namespace pypilot_event_loop
