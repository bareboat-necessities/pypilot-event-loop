#pragma once

#include <stdint.h>
#include "task.hpp"

namespace async_event_loop {

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
