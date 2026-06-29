#pragma once

#include <stddef.h>
#include <stdint.h>

#include "native.hpp"
#include "pin_io.hpp"
#include "scheduler.hpp"

#ifndef ASYNC_EVENT_LOOP_DEFAULT_MAX_CALLBACKS
#if defined(ARDUINO)
#define ASYNC_EVENT_LOOP_DEFAULT_MAX_CALLBACKS 32
#else
#define ASYNC_EVENT_LOOP_DEFAULT_MAX_CALLBACKS 128
#endif
#endif

#ifndef ASYNC_EVENT_LOOP_DEFAULT_CALLBACK_STORAGE_SIZE
#define ASYNC_EVENT_LOOP_DEFAULT_CALLBACK_STORAGE_SIZE 64
#endif

namespace async_event_loop {

class IByteStream {
public:
    virtual ~IByteStream() = default;

    virtual int read(uint8_t* dst, size_t max_len) = 0;
    virtual int write(const uint8_t* src, size_t len) = 0;

    virtual bool readable() const = 0;
    virtual bool writable() const = 0;
    virtual bool valid() const = 0;

    virtual bool is_datagram() const { return false; }
    virtual int native_fd() const { return -1; }
};

struct EventHandle {
    static constexpr uint16_t invalid_slot = 0xffffu;

    constexpr EventHandle() = default;
    constexpr EventHandle(uint16_t slot_value, uint16_t generation_value)
        : slot(slot_value), generation(generation_value) {}

    uint16_t slot = invalid_slot;
    uint16_t generation = 0;

    bool assigned() const { return slot != invalid_slot; }
    explicit operator bool() const { return assigned(); }
};

template<size_t MaxCallbacks = ASYNC_EVENT_LOOP_DEFAULT_MAX_CALLBACKS,
         size_t CallbackStorageSize = ASYNC_EVENT_LOOP_DEFAULT_CALLBACK_STORAGE_SIZE>
class EventLoop {
public:
    EventLoop() : scheduler_(clock_) {
        static_assert(MaxCallbacks <= 0xfffeu, "EventLoop supports at most 65534 callback slots");
    }

    static constexpr uint64_t default_readiness_poll_us = 1000ULL;

    bool valid() const { return scheduler_.valid(); }

    bool valid(EventHandle handle) const {
        return handle.assigned() &&
               handle.slot < MaxCallbacks &&
               callback_used_[handle.slot] &&
               callback_tasks_[handle.slot].active() &&
               callback_generations_[handle.slot] == handle.generation;
    }

    bool enable(EventHandle handle) {
        if (!valid(handle)) {
            return false;
        }
        callback_tasks_[handle.slot].set_enabled(true);
        return true;
    }

    bool disable(EventHandle handle) {
        if (!valid(handle)) {
            return false;
        }
        callback_tasks_[handle.slot].set_enabled(false);
        return true;
    }

    bool remove(EventHandle handle) {
        if (!valid(handle)) {
            return false;
        }
        if (!scheduler_.remove(callback_tasks_[handle.slot])) {
            return false;
        }
        release_registered(handle);
        return true;
    }

    bool add_periodic(IRuntimeTask& task, uint64_t period_us) {
        return scheduler_.add_periodic(task, period_us);
    }

    bool add_one_shot(IRuntimeTask& task, uint64_t due_us) {
        return scheduler_.add_one_shot(task, due_us);
    }

    template<typename Callable>
    EventHandle on_repeat_us(uint64_t period_us, Callable callable) {
        const EventHandle handle = allocate_callback(callable);
        if (!handle.assigned()) {
            return EventHandle{};
        }
        if (!scheduler_.add_periodic(callback_tasks_[handle.slot], period_us)) {
            release_unregistered(handle);
            return EventHandle{};
        }
        return handle;
    }

    template<typename Callable>
    EventHandle on_delay_us(uint64_t delay_us, Callable callable) {
        for (size_t i = 0; i < MaxCallbacks; ++i) {
            if (!callback_used_[i]) {
                const EventHandle handle{static_cast<uint16_t>(i), callback_generations_[i]};
                const bool set_ok = callback_tasks_[i].set(
                    OneShotCallback<Callable>(this, handle, static_cast<Callable&&>(callable)));
                if (!set_ok) {
                    return EventHandle{};
                }
                callback_used_[i] = true;
                if (!scheduler_.add_one_shot(callback_tasks_[i], clock_.micros() + delay_us)) {
                    release_unregistered(handle);
                    return EventHandle{};
                }
                return handle;
            }
        }
        return EventHandle{};
    }

    template<typename Callable>
    EventHandle on_repeat(uint32_t interval_ms, Callable callable) {
        return on_repeat_us(static_cast<uint64_t>(interval_ms) * 1000ULL, callable);
    }

    template<typename Callable>
    EventHandle on_delay(uint32_t delay_ms, Callable callable) {
        return on_delay_us(static_cast<uint64_t>(delay_ms) * 1000ULL, callable);
    }

    template<typename Callable>
    EventHandle on_bytes_ready(IByteStream& stream,
                               Callable callable,
                               uint64_t cooperative_poll_us = default_readiness_poll_us) {
        const EventHandle handle = allocate_callback([&stream, callable]() mutable {
            if (stream.readable()) {
                callable();
            }
        });
        if (!handle.assigned()) {
            return EventHandle{};
        }
        if (!register_readiness_or_poll(stream.native_fd(), callback_tasks_[handle.slot], cooperative_poll_us)) {
            release_unregistered(handle);
            return EventHandle{};
        }
        return handle;
    }

    template<typename Callable>
    EventHandle on_pin_event(IPinEventSource& source,
                             Callable callable,
                             uint64_t cooperative_poll_us = default_readiness_poll_us) {
        const EventHandle handle = allocate_callback([&source, callable]() mutable {
            PinEvent event;
            while (source.read_event(event)) {
                callable(event);
            }
        });
        if (!handle.assigned()) {
            return EventHandle{};
        }
        if (!register_readiness_or_poll(source.native_fd(), callback_tasks_[handle.slot], cooperative_poll_us)) {
            release_unregistered(handle);
            return EventHandle{};
        }
        return handle;
    }

    void tick() { scheduler_.run_once(); }
    void run_once() { scheduler_.run_once(); }
    void run_forever() { scheduler_.run_forever(); }
    void request_exit() { scheduler_.request_exit(); }

    NativeClock& clock() { return clock_; }
    NativeSchedulerFor<MaxCallbacks>& scheduler() { return scheduler_; }

private:
    template<typename Callable>
    struct OneShotCallback {
        OneShotCallback(EventLoop* loop_value, EventHandle handle_value, Callable callable_value)
            : loop(loop_value), handle(handle_value), callable(static_cast<Callable&&>(callable_value)) {}

        void operator()() {
            callable();
            loop->complete_one_shot(handle);
        }

        EventLoop* loop = nullptr;
        EventHandle handle;
        Callable callable;
    };

    template<typename Callable>
    EventHandle allocate_callback(Callable callable) {
        for (size_t i = 0; i < MaxCallbacks; ++i) {
            if (!callback_used_[i]) {
                if (!callback_tasks_[i].set(static_cast<Callable&&>(callable))) {
                    return EventHandle{};
                }
                callback_used_[i] = true;
                return EventHandle{static_cast<uint16_t>(i), callback_generations_[i]};
            }
        }
        return EventHandle{};
    }

    void release_registered(EventHandle handle) {
        if (!handle.assigned() || handle.slot >= MaxCallbacks) {
            return;
        }
        if (!callback_used_[handle.slot] || callback_generations_[handle.slot] != handle.generation) {
            return;
        }
        callback_tasks_[handle.slot].clear();
        callback_used_[handle.slot] = false;
        ++callback_generations_[handle.slot];
    }

    void complete_one_shot(EventHandle handle) {
        release_registered(handle);
    }

    void release_unregistered(EventHandle handle) {
        if (!handle.assigned() || handle.slot >= MaxCallbacks) {
            return;
        }
        callback_tasks_[handle.slot].clear();
        callback_used_[handle.slot] = false;
        ++callback_generations_[handle.slot];
    }

    bool register_readiness_or_poll(int fd, IRuntimeTask& task, uint64_t cooperative_poll_us) {
#if defined(__linux__)
        if (fd >= 0) {
            return scheduler_.add_readable_fd(fd, task);
        }
#else
        (void)fd;
#endif
        if (cooperative_poll_us == 0) {
            cooperative_poll_us = default_readiness_poll_us;
        }
        return scheduler_.add_periodic(task, cooperative_poll_us);
    }

    NativeClock clock_;
    NativeSchedulerFor<MaxCallbacks> scheduler_;
    CallbackTask<CallbackStorageSize> callback_tasks_[MaxCallbacks];
    uint16_t callback_generations_[MaxCallbacks]{};
    bool callback_used_[MaxCallbacks]{};
};

} // namespace async_event_loop
