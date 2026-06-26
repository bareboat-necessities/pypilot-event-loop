#pragma once

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
