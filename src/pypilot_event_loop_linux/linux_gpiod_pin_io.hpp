#pragma once

#if defined(__linux__) && defined(PYPILOT_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO)
#include <gpiod.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pypilot_event_loop/pin_io.hpp"

namespace pypilot_event_loop {

namespace detail {

inline bool gpiod_chip_path(const char* chip_name, char* dst, size_t dst_size) {
    if (!chip_name || !*chip_name || !dst || dst_size == 0) {
        return false;
    }
    if (chip_name[0] == '/') {
        snprintf(dst, dst_size, "%s", chip_name);
    } else {
        snprintf(dst, dst_size, "/dev/%s", chip_name);
    }
    return true;
}

inline struct gpiod_line_request* request_one_gpiod_line(const char* chip_name,
                                                         unsigned int line_offset,
                                                         enum gpiod_line_direction direction,
                                                         enum gpiod_line_value output_value,
                                                         const char* consumer) {
    char chip_path[128];
    if (!gpiod_chip_path(chip_name, chip_path, sizeof(chip_path))) {
        return nullptr;
    }

    struct gpiod_chip* chip = gpiod_chip_open(chip_path);
    if (!chip) {
        return nullptr;
    }

    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    struct gpiod_line_config* line_config = gpiod_line_config_new();
    struct gpiod_request_config* request_config = gpiod_request_config_new();
    struct gpiod_line_request* request = nullptr;

    if (!settings || !line_config || !request_config) {
        goto cleanup;
    }

    gpiod_line_settings_set_direction(settings, direction);
    if (direction == GPIOD_LINE_DIRECTION_OUTPUT) {
        gpiod_line_settings_set_output_value(settings, output_value);
    }

    if (gpiod_line_config_add_line_settings(line_config, &line_offset, 1, settings) != 0) {
        goto cleanup;
    }

    gpiod_request_config_set_consumer(request_config, consumer ? consumer : "pypilot_event_loop");
    request = gpiod_chip_request_lines(chip, request_config, line_config);

cleanup:
    if (request_config) {
        gpiod_request_config_free(request_config);
    }
    if (line_config) {
        gpiod_line_config_free(line_config);
    }
    if (settings) {
        gpiod_line_settings_free(settings);
    }
    gpiod_chip_close(chip);
    return request;
}

} // namespace detail

/**
 * Linux libgpiod v2 digital input pin.
 *
 * This backend claims the requested GPIO line through a v2 line request and
 * configures it as an input. The chip argument accepts either a full device path
 * such as `/dev/gpiochip0` or a chip name such as `gpiochip0`.
 */
class LinuxGpiodDigitalInputPin final : public IDigitalInputPin {
public:
    LinuxGpiodDigitalInputPin(const char* chip_name,
                              unsigned int line_offset,
                              const char* consumer = "pypilot_event_loop")
        : line_offset_(line_offset) {
        request_ = detail::request_one_gpiod_line(chip_name,
                                                  line_offset_,
                                                  GPIOD_LINE_DIRECTION_INPUT,
                                                  GPIOD_LINE_VALUE_INACTIVE,
                                                  consumer);
    }

    LinuxGpiodDigitalInputPin(const LinuxGpiodDigitalInputPin&) = delete;
    LinuxGpiodDigitalInputPin& operator=(const LinuxGpiodDigitalInputPin&) = delete;

    ~LinuxGpiodDigitalInputPin() override { close(); }

    bool valid() const override { return request_ != nullptr; }

    bool read() override {
        if (!request_) {
            return false;
        }
        const enum gpiod_line_value value = gpiod_line_request_get_value(request_, line_offset_);
        return value == GPIOD_LINE_VALUE_ACTIVE;
    }

private:
    void close() {
        if (request_) {
            gpiod_line_request_release(request_);
            request_ = nullptr;
        }
    }

    unsigned int line_offset_ = 0;
    struct gpiod_line_request* request_ = nullptr;
};

/**
 * Linux libgpiod v2 digital output pin.
 *
 * This backend claims the requested GPIO line through a v2 line request and
 * configures it as an output with the requested initial level.
 */
class LinuxGpiodDigitalOutputPin final : public IDigitalOutputPin {
public:
    LinuxGpiodDigitalOutputPin(const char* chip_name,
                               unsigned int line_offset,
                               bool initial = false,
                               const char* consumer = "pypilot_event_loop")
        : line_offset_(line_offset) {
        request_ = detail::request_one_gpiod_line(chip_name,
                                                  line_offset_,
                                                  GPIOD_LINE_DIRECTION_OUTPUT,
                                                  initial ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE,
                                                  consumer);
    }

    LinuxGpiodDigitalOutputPin(const LinuxGpiodDigitalOutputPin&) = delete;
    LinuxGpiodDigitalOutputPin& operator=(const LinuxGpiodDigitalOutputPin&) = delete;

    ~LinuxGpiodDigitalOutputPin() override { close(); }

    bool valid() const override { return request_ != nullptr; }

    bool write(bool high) override {
        if (!request_) {
            return false;
        }
        return gpiod_line_request_set_value(request_, line_offset_, high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE) == 0;
    }

private:
    void close() {
        if (request_) {
            gpiod_line_request_release(request_);
            request_ = nullptr;
        }
    }

    unsigned int line_offset_ = 0;
    struct gpiod_line_request* request_ = nullptr;
};

} // namespace pypilot_event_loop
#endif
