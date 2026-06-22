#pragma once

#include <stddef.h>
#include <stdint.h>

#include "byte_stream.hpp"
#include "callback_task.hpp"
#include "datagram_stream.hpp"
#include "event_handle.hpp"
#include "native.hpp"
#include "pin_event.hpp"

#ifndef PYPILOT_EVENT_LOOP_DEFAULT_MAX_CALLBACKS
#if defined(ARDUINO)
#define PYPILOT_EVENT_LOOP_DEFAULT_MAX_CALLBACKS 32
#else
#define PYPILOT_EVENT_LOOP_DEFAULT_MAX_CALLBACKS 128
#endif
#endif

#ifndef PYPILOT_EVENT_LOOP_DEFAULT_CALLBACK_STORAGE_SIZE
#define PYPILOT_EVENT_LOOP_DEFAULT_CALLBACK_STORAGE_SIZE 64
#endif

namespace pypilot_event_loop {

/**
 * Application-facing event-loop facade.
 *
 * EventLoop owns the native platform clock and scheduler selected by
 * native.hpp. On Linux this is the libevent scheduler. On Arduino this is the
 * cooperative scheduler. The public API is intentionally the same on both
 * platforms.
 *
 * Callback storage is fixed-size. This avoids std::function and heap-heavy
 * callback allocation in Arduino builds. Increase MaxCallbacks or
 * CallbackStorageSize when the application needs more/larger captured lambdas.
 * On Arduino, the default callback/task capacity stays 32 unless overridden;
 * non-Arduino builds default to 128. When MaxCallbacks is overridden on
 * Arduino, the cooperative scheduler capacity is bound to MaxCallbacks.
 *
 * Defaults can be overridden before including pypilot_event_loop.hpp:
 *
 *   #define PYPILOT_EVENT_LOOP_DEFAULT_MAX_CALLBACKS 16
 *   #define PYPILOT_EVENT_LOOP_DEFAULT_CALLBACK_STORAGE_SIZE 48
 *   #include <pypilot_event_loop.hpp>
 */
template<size_t MaxCallbacks = PYPILOT_EVENT_LOOP_DEFAULT_MAX_CALLBACKS,
         size_t CallbackStorageSize = PYPILOT_EVENT_LOOP_DEFAULT_CALLBACK_STORAGE_SIZE>
class EventLoop {
public:
    EventLoop() : scheduler_(clock_) {
        static_assert(MaxCallbacks <= 0xfffeu, "EventLoop supports at most 65534 callback slots");
    }

    /** Default cooperative readiness check period for streams or pins without fd readiness. */
    static constexpr uint64_t default_readiness_poll_us = 1000ULL;

    /** Return true when the selected platform backend initialized correctly. */
    bool valid() const { return scheduler_.valid(); }

    /** Return true when an event handle still names an active slot in this loop. */
    bool valid(EventHandle handle) const {
        return handle.assigned() &&
               handle.slot < MaxCallbacks &&
               callback_used_[handle.slot] &&
               callback_tasks_[handle.slot].active() &&
               callback_generations_[handle.slot] == handle.generation;
    }

    /** Enable a previously registered callback slot. */
    bool enable(EventHandle handle) {
        if (!valid(handle)) {
            return false;
        }
        callback_tasks_[handle.slot].set_enabled(true);
        return true;
    }

    /** Disable a callback slot without destroying its stored callable. */
    bool disable(EventHandle handle) {
        if (!valid(handle)) {
            return false;
        }
        callback_tasks_[handle.slot].set_enabled(false);
        return true;
    }

    /** Remove a callback slot, unschedule its backend task, and make the slot reusable. */
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

    /** Register a named runtime task that repeats every period_us microseconds. */
    bool add_periodic(IRuntimeTask& task, uint64_t period_us) {
        return scheduler_.add_periodic(task, period_us);
    }

    /** Register a named runtime task that runs once at absolute due_us. */
    bool add_one_shot(IRuntimeTask& task, uint64_t due_us) {
        return scheduler_.add_one_shot(task, due_us);
    }

    /** Register a lambda/callable that repeats every period_us microseconds. */
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

    /**
     * Register a lambda/callable that runs once after delay_us microseconds.
     *
     * The stored callable is wrapped with a small one-shot completion object.
     * This avoids a nested lambda capture and gives a clearer compile-time
     * storage-size failure when CallbackStorageSize is too small.
     */
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

    /** Register a lambda/callable that repeats every interval_ms milliseconds. */
    template<typename Callable>
    EventHandle on_repeat(uint32_t interval_ms, Callable callable) {
        return on_repeat_us(static_cast<uint64_t>(interval_ms) * 1000ULL, callable);
    }

    /** Register a lambda/callable that runs once after delay_ms milliseconds. */
    template<typename Callable>
    EventHandle on_delay(uint32_t delay_ms, Callable callable) {
        return on_delay_us(static_cast<uint64_t>(delay_ms) * 1000ULL, callable);
    }

    /**
     * Register a byte-stream data-ready callback.
     *
     * Linux fd-backed streams are registered with the native libevent fd
     * readiness backend. Streams without a native fd, such as Arduino Stream
     * wrappers or static test streams, are checked cooperatively from tick().
     *
     * The stream object is captured by reference and must outlive the returned
     * EventHandle or be removed before the stream is destroyed.
     */
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

    /**
     * Register a datagram-stream data-ready callback.
     *
     * The datagram stream object is captured by reference and must outlive the
     * returned EventHandle or be removed before the stream is destroyed.
     */
    template<typename Callable>
    EventHandle on_bytes_ready(IDatagramStream& stream,
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

    /**
     * Register a GPIO/pin event source callback.
     *
     * Linux gpiod-backed sources return a native fd and are integrated with
     * libevent readiness. Cooperative sources return -1 and are checked from
     * tick(). The callable must accept `const PinEvent&`.
     *
     * The event source object is captured by reference and must outlive the
     * returned EventHandle or be removed before the source is destroyed.
     */
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

    /** Poll one loop iteration. This is the method normally called by Arduino loop(). */
    void tick() { scheduler_.run_once(); }

    /** Poll one loop iteration. Same as tick(), but clearer in Linux code. */
    void run_once() { scheduler_.run_once(); }

    /** Run until request_exit() is called. Normal Linux daemon/app entry point. */
    void run_forever() { scheduler_.run_forever(); }

    /** Request exit from run_forever(). */
    void request_exit() { scheduler_.request_exit(); }

    /** Access the native clock for advanced integrations/tests. */
    NativeClock& clock() { return clock_; }

    /** Access the native scheduler for fd readiness or lower-level integrations. */
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

} // namespace pypilot_event_loop
