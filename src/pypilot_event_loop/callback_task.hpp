#pragma once

#include <new>
#include <stddef.h>
#include <stdint.h>

#include "task.hpp"

namespace pypilot_event_loop {

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
        return true;
    }

    void poll(uint64_t now_us) override {
        if (active_ && invoke_) {
            invoke_(storage_, now_us);
        }
    }

    bool active() const { return active_; }

    void clear() {
        if (active_ && destroy_) {
            destroy_(storage_);
        }
        active_ = false;
        invoke_ = nullptr;
        destroy_ = nullptr;
    }

private:
    alignas(void*) unsigned char storage_[StorageSize]{};
    void (*invoke_)(void*, uint64_t) = nullptr;
    void (*destroy_)(void*) = nullptr;
    bool active_ = false;
};

} // namespace pypilot_event_loop
