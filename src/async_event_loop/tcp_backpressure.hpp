#pragma once

#include <stddef.h>
#include <stdint.h>

#include "tcp.hpp"

namespace async_event_loop {

struct TcpBackpressureOptions {
    bool enabled = false;
    size_t high_water_bytes = 32768;
    size_t low_water_bytes = 8192;
    uint32_t max_over_high_polls = 3;
    bool close_on_limit = true;

    static TcpBackpressureOptions disabled() {
        return TcpBackpressureOptions{};
    }

    static TcpBackpressureOptions server_default() {
        TcpBackpressureOptions options;
        options.enabled = true;
        return options;
    }

    static TcpBackpressureOptions embedded_default() {
        TcpBackpressureOptions options;
        options.enabled = true;
        options.high_water_bytes = 768;
        options.low_water_bytes = 256;
        options.max_over_high_polls = 2;
        return options;
    }
};

struct TcpBackpressureInfo {
    size_t pending_bytes = 0;
    size_t high_water_bytes = 0;
    size_t low_water_bytes = 0;
    uint32_t over_high_polls = 0;
    bool close_on_limit = false;
};

class TcpBackpressureMonitor final {
public:
    TcpBackpressureMonitor() = default;
    explicit TcpBackpressureMonitor(const TcpBackpressureOptions& options) : options_(options) {}

    void configure(const TcpBackpressureOptions& options) {
        options_ = options;
        reset();
    }

    const TcpBackpressureOptions& options() const { return options_; }
    bool enabled() const { return options_.enabled; }

    void reset() {
        over_high_polls_ = 0;
        reported_ = false;
    }

    bool check(ITcpConnection& connection, TcpBackpressureInfo* info = nullptr) {
        if (!options_.enabled || !connection.valid()) {
            fill_info(connection, info);
            return false;
        }

        const size_t pending = connection.output_size();
        if (pending > options_.high_water_bytes) {
            if (over_high_polls_ < UINT32_MAX) {
                ++over_high_polls_;
            }
        } else if (pending <= options_.low_water_bytes) {
            over_high_polls_ = 0;
            reported_ = false;
        }

        fill_info(connection, info);

        const uint32_t trigger_polls = options_.max_over_high_polls == 0 ? 1 : options_.max_over_high_polls;
        if (over_high_polls_ >= trigger_polls && !reported_) {
            reported_ = true;
            return true;
        }
        return false;
    }

private:
    void fill_info(ITcpConnection& connection, TcpBackpressureInfo* info) const {
        if (!info) {
            return;
        }
        info->pending_bytes = connection.valid() ? connection.output_size() : 0;
        info->high_water_bytes = options_.high_water_bytes;
        info->low_water_bytes = options_.low_water_bytes;
        info->over_high_polls = over_high_polls_;
        info->close_on_limit = options_.close_on_limit;
    }

    TcpBackpressureOptions options_;
    uint32_t over_high_polls_ = 0;
    bool reported_ = false;
};

} // namespace async_event_loop
