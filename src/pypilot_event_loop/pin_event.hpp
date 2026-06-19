#pragma once

#include <stdint.h>

#include "callback_task.hpp"
#include "task.hpp"

namespace pypilot_event_loop {

/**
 * Abstract digital input pin.
 *
 * This is intentionally not Arduino-specific. A Linux/Raspberry Pi backend can
 * later implement this interface using GPIO character devices, libgpiod, sysfs,
 * or another board-specific input source. Arduino can implement it with
 * digitalRead().
 */
class IPinInput {
public:
    virtual ~IPinInput() = default;

    /** Return true when the input source can be sampled. */
    virtual bool valid() const = 0;

    /** Return the current digital level. false = low, true = high. */
    virtual bool read() const = 0;

    /** Optional stable identifier for logs/tests. */
    virtual uint32_t id() const = 0;
};

/** Digital pin event modes supported by PinEventTask. */
enum class PinEventType : uint8_t {
    RisingEdge,
    FallingEdge,
    Change,
    HighLevel,
    LowLevel
};

/**
 * Polling pin-event task.
 *
 * The task samples an IPinInput each time poll() is called. When the configured
 * edge or level condition is detected, it invokes another IRuntimeTask.
 *
 * This keeps GPIO handling portable. The event-loop backend decides when this
 * task is polled: periodically, from a platform interrupt shim, or from a
 * Linux GPIO readiness wrapper in a later backend.
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

/**
 * Lambda-backed pin-event task.
 *
 * This is the lambda analogue of PinEventTask. It owns a fixed-storage
 * CallbackTask and forwards matching pin events to the stored callable.
 */
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

    void poll(uint64_t now_us) override {
        pin_task_.poll(now_us);
    }

    PinEventTask& pin_task() { return pin_task_; }

private:
    CallbackTask<CallbackStorageSize> callback_;
    PinEventTask pin_task_;
};

} // namespace pypilot_event_loop
