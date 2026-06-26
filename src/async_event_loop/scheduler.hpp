#pragma once

#include <new>
#include <stddef.h>
#include <stdint.h>

namespace async_event_loop {

class IClock {
public:
    virtual ~IClock() = default;
    virtual uint64_t micros() const = 0;
};

class IRuntimeTask {
public:
    virtual ~IRuntimeTask() = default;
    virtual void poll(uint64_t now_us) = 0;
};

/**
 * Fixed-storage callback runtime task.
 *
 * CallbackTask owns one callable inside static storage. It can be disabled or
 * removed after it has been registered with a scheduler. Destruction is deferred
 * when a callback removes itself while its callable is still executing.
 */
template<size_t StorageSize = 48>
class CallbackTask final : public IRuntimeTask {
public:
    CallbackTask() = default;

    CallbackTask(const CallbackTask&) = delete;
    CallbackTask& operator=(const CallbackTask&) = delete;

    ~CallbackTask() override { destroy_now(); }

    template<typename Callable>
    bool set(Callable callable) {
        static_assert(sizeof(Callable) <= StorageSize, "callback object is too large for CallbackTask storage");
        static_assert(alignof(Callable) <= alignof(StorageAlignment), "callback object alignment is too large for CallbackTask storage");
        clear();
        if (invoking_) {
            return false;
        }
        new (storage_) Callable(static_cast<Callable&&>(callable));
        invoke_ = [](void* storage, uint64_t) {
            (*static_cast<Callable*>(storage))();
        };
        destroy_ = [](void* storage) {
            static_cast<Callable*>(storage)->~Callable();
        };
        active_ = true;
        enabled_ = true;
        clear_requested_ = false;
        return true;
    }

    void poll(uint64_t now_us) override {
        if (active_ && enabled_ && invoke_) {
            invoking_ = true;
            void (*invoke)(void*, uint64_t) = invoke_;
            invoke(storage_, now_us);
            invoking_ = false;
            if (clear_requested_) {
                destroy_now();
            }
        }
    }

    bool active() const { return active_; }
    bool enabled() const { return enabled_; }

    void set_enabled(bool enabled) {
        if (active_) {
            enabled_ = enabled;
        }
    }

    void clear() {
        if (invoking_) {
            active_ = false;
            enabled_ = false;
            clear_requested_ = true;
            return;
        }
        destroy_now();
    }

private:
    union StorageAlignment {
        void* pointer;
        long long integer;
        double floating;
        long double long_floating;
    };

    void destroy_now() {
        if ((active_ || clear_requested_) && destroy_) {
            destroy_(storage_);
        }
        active_ = false;
        enabled_ = false;
        clear_requested_ = false;
        invoke_ = nullptr;
        destroy_ = nullptr;
    }

    alignas(StorageAlignment) unsigned char storage_[StorageSize]{};
    void (*invoke_)(void*, uint64_t) = nullptr;
    void (*destroy_)(void*) = nullptr;
    bool active_ = false;
    bool enabled_ = false;
    bool invoking_ = false;
    bool clear_requested_ = false;
};

class PeriodicTimer {
public:
    explicit PeriodicTimer(uint64_t period_us = 0) : period_us_(period_us) {}

    void set_period_us(uint64_t period_us) { period_us_ = period_us; }
    uint64_t period_us() const { return period_us_; }

    void reset(uint64_t now_us = 0) {
        next_due_us_ = now_us;
        armed_ = true;
    }

    bool ready(uint64_t now_us) {
        if (!armed_) {
            reset(now_us);
        }
        if (period_us_ == 0 || now_us < next_due_us_) {
            return false;
        }
        do {
            next_due_us_ += period_us_;
        } while (now_us >= next_due_us_);
        return true;
    }

    uint64_t next_due_us() const { return next_due_us_; }

private:
    uint64_t period_us_ = 0;
    uint64_t next_due_us_ = 0;
    bool armed_ = false;
};

class OneShotTimer {
public:
    void arm(uint64_t due_us) {
        due_us_ = due_us;
        armed_ = true;
        fired_ = false;
    }

    void disarm() { armed_ = false; }
    bool armed() const { return armed_; }
    bool fired() const { return fired_; }
    uint64_t due_us() const { return due_us_; }

    bool ready(uint64_t now_us) {
        if (!armed_ || fired_ || now_us < due_us_) {
            return false;
        }
        fired_ = true;
        armed_ = false;
        return true;
    }

private:
    uint64_t due_us_ = 0;
    bool armed_ = false;
    bool fired_ = false;
};

class IScheduler {
public:
    virtual ~IScheduler() = default;

    virtual bool valid() const = 0;

    virtual bool add_periodic(IRuntimeTask& task, uint64_t period_us) = 0;
    virtual bool add_one_shot(IRuntimeTask& task, uint64_t due_us) = 0;
    virtual bool remove(IRuntimeTask& task) = 0;

    virtual void run_once() = 0;
    virtual void run_forever() = 0;
    virtual void request_exit() = 0;
};

} // namespace async_event_loop
