#pragma once

#include <new>
#include <stddef.h>
#include <stdint.h>

#include "task.hpp"

namespace pypilot_event_loop {

/**
 * Fixed-storage callback runtime task.
 *
 * CallbackTask owns one callable inside static storage. It can be disabled or
 * removed after it has been registered with a scheduler; disabled/removed tasks
 * remain harmless even if the backend still has a reference to the task object.
 */
template<size_t StorageSize = 48>
class CallbackTask final : public IRuntimeTask {
public:
    CallbackTask() = default;

    CallbackTask(const CallbackTask&) = delete;
    CallbackTask& operator=(const CallbackTask&) = delete;

    ~CallbackTask() override { clear(); }

    template<typename Callable>
    bool set(Callable callable) {
        static_assert(sizeof(Callable) <= StorageSize, "callback object is too large for CallbackTask storage");
        clear();
        new (storage_) Callable(callable);
        invoke_ = [](void* storage, uint64_t) {
            (*static_cast<Callable*>(storage))();
        };
        destroy_ = [](void* storage) {
            static_cast<Callable*>(storage)->~Callable();
        };
        active_ = true;
        enabled_ = true;
        return true;
    }

    void poll(uint64_t now_us) override {
        if (active_ && enabled_ && invoke_) {
            invoke_(storage_, now_us);
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
        if (active_ && destroy_) {
            destroy_(storage_);
        }
        active_ = false;
        enabled_ = false;
        invoke_ = nullptr;
        destroy_ = nullptr;
    }

private:
    alignas(void*) unsigned char storage_[StorageSize]{};
    void (*invoke_)(void*, uint64_t) = nullptr;
    void (*destroy_)(void*) = nullptr;
    bool active_ = false;
    bool enabled_ = false;
};

} // namespace pypilot_event_loop
