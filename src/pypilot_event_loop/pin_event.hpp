#pragma once

#include <stdint.h>

#include "callback_task.hpp"
#include "task.hpp"

namespace pypilot_event_loop {

/**
 * Abstract sampled digital input pin.
 *
 * Use this for cooperative pin polling, Arduino digitalRead() wrappers, and
 * test pins. Production Linux edge events should normally use IPinEventSource
 * instead, so the event loop can watch a file descriptor.
 */
class IPinInput {
public:
    virtual ~IPinInput() = default;
    virtual bool valid() const = 0;
    virtual bool read() const = 0;
    virtual uint32_t id() const = 0;
};

/** Digital pin event modes. */
enum class PinEventType : uint8_t {
    RisingEdge,
    FallingEdge,
    Change,
    HighLevel,
    LowLevel
};

/** Normalized pin event record produced by IPinEventSource. */
struct PinEvent {
    PinEventType type = PinEventType::Change;
    uint64_t timestamp_us = 0;
    uint32_t source_id = 0;
    uint32_t sequence = 0;
    bool level = false;
};

/**
 * Abstract edge-event source for GPIO-like inputs.
 *
 * Linux gpiod sources return a native fd and are integrated with libevent.
 * Arduino interrupt/cooperative sources may return -1 and are checked from
 * tick(). read_event() must drain one pending event and return true when one
 * was available.
 */
class IPinEventSource {
public:
    virtual ~IPinEventSource() = default;
    virtual bool valid() const = 0;
    virtual int native_fd() const { return -1; }
    virtual bool read_event(PinEvent& event) = 0;
};

/**
 * Polling pin-event task for IPinInput.
 *
 * This is still useful for Arduino or test code when a real edge-event fd is
 * not available. Prefer IPinEventSource for production Linux GPIO edge events.
 */
class PinEventTask final : public IRuntimeTask {
public:
    PinEventTask(IPinInput& pin,
                 IRuntimeTask& target,
                 PinEventType event_type = PinEventType::Change,
                 uint64_t debounce_us = 0)
        : pin_(pin), target_(target), event_type_(event_type), debounce_us_(debounce_us) {}

    void poll(uint64_t now_us) override {
        if (!pin_.valid()) {
            return;
        }

        const bool current = pin_.read();
        if (!has_last_) {
            last_level_ = current;
            has_last_ = true;
            if (is_level_event(current) && debounce_allows(now_us)) {
                target_.poll(now_us);
            }
            return;
        }

        const bool triggered = event_matches(last_level_, current);
        last_level_ = current;

        if (triggered && debounce_allows(now_us)) {
            target_.poll(now_us);
        }
    }

    PinEventType event_type() const { return event_type_; }
    uint64_t debounce_us() const { return debounce_us_; }
    bool last_level() const { return last_level_; }
    bool has_last_level() const { return has_last_; }

private:
    bool is_level_event(bool current) const {
        return (event_type_ == PinEventType::HighLevel && current) ||
               (event_type_ == PinEventType::LowLevel && !current);
    }

    bool event_matches(bool previous, bool current) const {
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

    bool debounce_allows(uint64_t now_us) {
        if (!has_event_) {
            last_event_us_ = now_us;
            has_event_ = true;
            return true;
        }
        if (debounce_us_ == 0 || now_us - last_event_us_ >= debounce_us_) {
            last_event_us_ = now_us;
            return true;
        }
        return false;
    }

    IPinInput& pin_;
    IRuntimeTask& target_;
    PinEventType event_type_ = PinEventType::Change;
    uint64_t debounce_us_ = 0;
    uint64_t last_event_us_ = 0;
    bool has_event_ = false;
    bool last_level_ = false;
    bool has_last_ = false;
};

template<size_t CallbackStorageSize = 48>
class LambdaPinEventTask final : public IRuntimeTask {
public:
    template<typename Callable>
    LambdaPinEventTask(IPinInput& pin,
                       PinEventType event_type,
                       Callable callable,
                       uint64_t debounce_us = 0)
        : callback_(), pin_task_(pin, callback_, event_type, debounce_us) {
        callback_.set(callable);
    }

    void poll(uint64_t now_us) override { pin_task_.poll(now_us); }
    PinEventTask& pin_task() { return pin_task_; }

private:
    CallbackTask<CallbackStorageSize> callback_;
    PinEventTask pin_task_;
};

} // namespace pypilot_event_loop
