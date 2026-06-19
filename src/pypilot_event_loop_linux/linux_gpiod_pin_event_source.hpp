#pragma once

#include <gpiod.h>
#include <stdint.h>
#include <stdlib.h>

#include "pypilot_event_loop/pin_event.hpp"

namespace pypilot_event_loop {

/**
 * Linux GPIO edge-event source backed by libgpiod v2.
 *
 * The requested line produces an fd via gpiod_line_request_get_fd(). EventLoop
 * registers that fd with libevent through on_pin_event(). read_event() drains
 * one edge event from libgpiod's edge-event buffer.
 */
class LinuxGpiodPinEventSource final : public IPinEventSource {
public:
    LinuxGpiodPinEventSource(const char* chip_path,
                             unsigned int line_offset,
                             PinEventType event_type,
                             const char* consumer = "pypilot-event-loop",
                             unsigned long debounce_us = 0,
                             size_t event_buffer_capacity = 16)
        : line_offset_(line_offset), requested_type_(event_type) {
        open(chip_path, consumer, debounce_us, event_buffer_capacity);
    }

    LinuxGpiodPinEventSource(const LinuxGpiodPinEventSource&) = delete;
    LinuxGpiodPinEventSource& operator=(const LinuxGpiodPinEventSource&) = delete;

    ~LinuxGpiodPinEventSource() override {
        if (buffer_) {
            gpiod_edge_event_buffer_free(buffer_);
        }
        if (request_) {
            gpiod_line_request_release(request_);
        }
        if (chip_) {
            gpiod_chip_close(chip_);
        }
    }

    bool valid() const override { return request_ != nullptr && buffer_ != nullptr; }

    int native_fd() const override {
        if (!request_) {
            return -1;
        }
        return gpiod_line_request_get_fd(request_);
    }

    bool read_event(PinEvent& event) override {
        if (!valid()) {
            return false;
        }

        const int n = gpiod_line_request_read_edge_events(request_, buffer_, 1);
        if (n <= 0) {
            return false;
        }

        struct gpiod_edge_event* gevent = gpiod_edge_event_buffer_get_event(buffer_, 0);
        if (!gevent) {
            return false;
        }

        const enum gpiod_edge_event_type type = gpiod_edge_event_get_event_type(gevent);
        event.type = (type == GPIOD_EDGE_EVENT_RISING_EDGE) ? PinEventType::RisingEdge
                                                            : PinEventType::FallingEdge;
        event.timestamp_us = gpiod_edge_event_get_timestamp_ns(gevent) / 1000ULL;
        event.source_id = gpiod_edge_event_get_line_offset(gevent);
        event.sequence = static_cast<uint32_t>(gpiod_edge_event_get_global_seqno(gevent));
        event.level = event.type == PinEventType::RisingEdge;
        return true;
    }

    unsigned int line_offset() const { return line_offset_; }
    PinEventType requested_type() const { return requested_type_; }

private:
    static enum gpiod_line_edge to_gpiod_edge(PinEventType type) {
        switch (type) {
        case PinEventType::RisingEdge:
            return GPIOD_LINE_EDGE_RISING;
        case PinEventType::FallingEdge:
            return GPIOD_LINE_EDGE_FALLING;
        case PinEventType::Change:
            return GPIOD_LINE_EDGE_BOTH;
        case PinEventType::HighLevel:
        case PinEventType::LowLevel:
            return GPIOD_LINE_EDGE_BOTH;
        }
        return GPIOD_LINE_EDGE_BOTH;
    }

    void open(const char* chip_path,
              const char* consumer,
              unsigned long debounce_us,
              size_t event_buffer_capacity) {
        chip_ = gpiod_chip_open(chip_path);
        if (!chip_) {
            return;
        }

        struct gpiod_line_settings* settings = gpiod_line_settings_new();
        struct gpiod_line_config* line_config = gpiod_line_config_new();
        struct gpiod_request_config* request_config = gpiod_request_config_new();
        if (!settings || !line_config || !request_config) {
            cleanup_setup(settings, line_config, request_config);
            return;
        }

        gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
        gpiod_line_settings_set_edge_detection(settings, to_gpiod_edge(requested_type_));
        if (debounce_us != 0) {
            gpiod_line_settings_set_debounce_period_us(settings, debounce_us);
        }

        unsigned int offset = line_offset_;
        if (gpiod_line_config_add_line_settings(line_config, &offset, 1, settings) != 0) {
            cleanup_setup(settings, line_config, request_config);
            return;
        }

        gpiod_request_config_set_consumer(request_config, consumer);
        request_ = gpiod_chip_request_lines(chip_, request_config, line_config);
        cleanup_setup(settings, line_config, request_config);

        if (!request_) {
            return;
        }

        buffer_ = gpiod_edge_event_buffer_new(event_buffer_capacity);
    }

    static void cleanup_setup(struct gpiod_line_settings* settings,
                              struct gpiod_line_config* line_config,
                              struct gpiod_request_config* request_config) {
        if (request_config) {
            gpiod_request_config_free(request_config);
        }
        if (line_config) {
            gpiod_line_config_free(line_config);
        }
        if (settings) {
            gpiod_line_settings_free(settings);
        }
    }

    struct gpiod_chip* chip_ = nullptr;
    struct gpiod_line_request* request_ = nullptr;
    struct gpiod_edge_event_buffer* buffer_ = nullptr;
    unsigned int line_offset_ = 0;
    PinEventType requested_type_ = PinEventType::Change;
};

} // namespace pypilot_event_loop
