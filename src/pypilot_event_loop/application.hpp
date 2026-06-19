#pragma once

#include <stddef.h>
#include <stdint.h>

#include "callback_task.hpp"
#include "native.hpp"

namespace pypilot_event_loop {

template<size_t MaxCallbacks = 32, size_t CallbackStorageSize = 48>
class EventLoop {
public:
    EventLoop() : scheduler_(clock_) {}

    bool valid() const { return scheduler_.valid(); }

    bool add_periodic(IRuntimeTask& task, uint64_t period_us) {
        return scheduler_.add_periodic(task, period_us);
    }

    bool add_one_shot(IRuntimeTask& task, uint64_t due_us) {
        return scheduler_.add_one_shot(task, due_us);
    }

    template<typename Callable>
    bool on_repeat_us(uint64_t period_us, Callable callable) {
        CallbackTask<CallbackStorageSize>* task = allocate_callback(callable);
        if (!task) {
            return false;
        }
        return scheduler_.add_periodic(*task, period_us);
    }

    template<typename Callable>
    bool on_delay_us(uint64_t delay_us, Callable callable) {
        CallbackTask<CallbackStorageSize>* task = allocate_callback(callable);
        if (!task) {
            return false;
        }
        return scheduler_.add_one_shot(*task, clock_.micros() + delay_us);
    }

    template<typename Callable>
    bool onRepeat(uint32_t interval_ms, Callable callable) {
        return on_repeat_us(static_cast<uint64_t>(interval_ms) * 1000ULL, callable);
    }

    template<typename Callable>
    bool onDelay(uint32_t delay_ms, Callable callable) {
        return on_delay_us(static_cast<uint64_t>(delay_ms) * 1000ULL, callable);
    }

    void tick() { scheduler_.run_once(); }
    void run_once() { scheduler_.run_once(); }
    void run_forever() { scheduler_.run_forever(); }
    void request_exit() { scheduler_.request_exit(); }

    NativeClock& clock() { return clock_; }
    NativeScheduler& scheduler() { return scheduler_; }

private:
    template<typename Callable>
    CallbackTask<CallbackStorageSize>* allocate_callback(Callable callable) {
        for (size_t i = 0; i < MaxCallbacks; ++i) {
            if (!callback_used_[i]) {
                callback_tasks_[i].set(callable);
                callback_used_[i] = true;
                return &callback_tasks_[i];
            }
        }
        return nullptr;
    }

    NativeClock clock_;
    NativeScheduler scheduler_;
    CallbackTask<CallbackStorageSize> callback_tasks_[MaxCallbacks];
    bool callback_used_[MaxCallbacks]{};
};

} // namespace pypilot_event_loop
