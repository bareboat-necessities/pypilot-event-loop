#pragma once

#include <stddef.h>
#include <stdint.h>

#include "byte_stream.hpp"
#include "callback_task.hpp"
#include "datagram_stream.hpp"
#include "native.hpp"
#include "pin_event.hpp"

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
 */
template<size_t MaxCallbacks = 32, size_t CallbackStorageSize = 64>
class EventLoop {
public:
    EventLoop() : scheduler_(clock_) {}

    /** Default cooperative readiness check period for streams or pins without fd readiness. */
    static constexpr uint64_t default_readable_poll_us = 1000ULL;

    /** Return true when the selected platform backend initialized correctly. */
    bool valid() const { return scheduler_.valid(); }

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
    bool on_repeat_us(uint64_t period_us, Callable callable) {
        CallbackTask<CallbackStorageSize>* task = allocate_callback(callable);
        if (!task) {
            return false;
        }
        return scheduler_.add_periodic(*task, period_us);
    }

    /** Register a lambda/callable that runs once after delay_us microseconds. */
    template<typename Callable>
    bool on_delay_us(uint64_t delay_us, Callable callable) {
        CallbackTask<CallbackStorageSize>* task = allocate_callback(callable);
        if (!task) {
            return false;
        }
        return scheduler_.add_one_shot(*task, clock_.micros() + delay_us);
    }

    /** Register a lambda/callable that repeats every interval_ms milliseconds. */
    template<typename Callable>
    bool on_repeat(uint32_t interval_ms, Callable callable) {
        return on_repeat_us(static_cast<uint64_t>(interval_ms) * 1000ULL, callable);
    }

    /** Register a lambda/callable that runs once after delay_ms milliseconds. */
    template<typename Callable>
    bool on_delay(uint32_t delay_ms, Callable callable) {
        return on_delay_us(static_cast<uint64_t>(delay_ms) * 1000ULL, callable);
    }

    /**
     * Register a byte-stream readable callback.
     *
     * Linux fd-backed streams are registered with the native libevent fd
     * readiness backend. Streams without a native fd, such as Arduino Stream
     * wrappers or static test streams, are checked cooperatively from tick().
     */
    template<typename Callable>
    bool on_readable(IByteStream& stream,
                     Callable callable,
                     uint64_t cooperative_poll_us = default_readable_poll_us) {
        CallbackTask<CallbackStorageSize>* task = allocate_callback([&stream, callable]() mutable {
            if (stream.readable()) {
                callable();
            }
        });
        if (!task) {
            return false;
        }
        return register_readable_or_poll(stream.native_fd(), *task, cooperative_poll_us);
    }

    /**
     * Register a datagram-stream readable callback.
     *
     * Linux fd-backed datagram streams are registered with libevent fd
     * readiness. Streams without a native fd are checked cooperatively.
     */
    template<typename Callable>
    bool on_readable(IDatagramStream& stream,
                     Callable callable,
                     uint64_t cooperative_poll_us = default_readable_poll_us) {
        CallbackTask<CallbackStorageSize>* task = allocate_callback([&stream, callable]() mutable {
            if (stream.readable()) {
                callable();
            }
        });
        if (!task) {
            return false;
        }
        return register_readable_or_poll(stream.native_fd(), *task, cooperative_poll_us);
    }

    /**
     * Register a GPIO/pin event source callback.
     *
     * Linux gpiod-backed sources return a native fd and are integrated with
     * libevent readiness. Cooperative sources return -1 and are checked from
     * tick(). The callable must accept `const PinEvent&`.
     */
    template<typename Callable>
    bool on_pin_event(IPinEventSource& source,
                      Callable callable,
                      uint64_t cooperative_poll_us = default_readable_poll_us) {
        CallbackTask<CallbackStorageSize>* task = allocate_callback([&source, callable]() mutable {
            PinEvent event;
            while (source.read_event(event)) {
                callable(event);
            }
        });
        if (!task) {
            return false;
        }
        return register_readable_or_poll(source.native_fd(), *task, cooperative_poll_us);
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

    bool register_readable_or_poll(int fd, IRuntimeTask& task, uint64_t cooperative_poll_us) {
#if defined(__linux__)
        if (fd >= 0) {
            return scheduler_.add_readable_fd(fd, task);
        }
#else
        (void)fd;
#endif
        if (cooperative_poll_us == 0) {
            cooperative_poll_us = default_readable_poll_us;
        }
        return scheduler_.add_periodic(task, cooperative_poll_us);
    }

    NativeClock clock_;
    NativeScheduler scheduler_;
    CallbackTask<CallbackStorageSize> callback_tasks_[MaxCallbacks];
    bool callback_used_[MaxCallbacks]{};
};

} // namespace pypilot_event_loop
