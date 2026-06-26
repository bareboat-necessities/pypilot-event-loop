#pragma once

#include <new>
#include <stddef.h>
#include <stdint.h>

#include "scheduler.hpp"

namespace async_event_loop {

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

} // namespace async_event_loop
