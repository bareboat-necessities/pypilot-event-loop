#pragma once

#include <stddef.h>
#include "pypilot_event_loop/fixed_vector.hpp"
#include "pypilot_event_loop/scheduler.hpp"
#include "pypilot_event_loop/clock.hpp"

namespace pypilot_event_loop {

class ArduinoLoop final : public IScheduler {
public:
    explicit ArduinoLoop(IClock& clock) : clock_(clock) {}

    bool add_periodic(IRuntimeTask& task, uint64_t period_us) override {
        if (period_us == 0) {
            return false;
        }
        TaskSlot slot;
        slot.task = &task;
        slot.period_us = period_us;
        slot.next_due_us = clock_.micros();
        slot.periodic = true;
        slot.active = true;
        return tasks_.push_back(slot);
    }

    bool add_one_shot(IRuntimeTask& task, uint64_t due_us) override {
        TaskSlot slot;
        slot.task = &task;
        slot.period_us = 0;
        slot.next_due_us = due_us;
        slot.periodic = false;
        slot.active = true;
        return tasks_.push_back(slot);
    }

    void run_once() override {
        const uint64_t now_us = clock_.micros();
        for (size_t i = 0; i < tasks_.size(); ++i) {
            TaskSlot& slot = tasks_[i];
            if (!slot.active || !slot.task || now_us < slot.next_due_us) {
                continue;
            }
            slot.task->poll(now_us);
            if (slot.periodic) {
                do {
                    slot.next_due_us += slot.period_us;
                } while (now_us >= slot.next_due_us);
            } else {
                slot.active = false;
            }
        }
    }

    void run_forever() override {
        while (!exit_requested_) {
            run_once();
            yield_if_available();
        }
    }

    void request_exit() override { exit_requested_ = true; }
    void tick() { run_once(); }

private:
    struct TaskSlot {
        IRuntimeTask* task = nullptr;
        uint64_t period_us = 0;
        uint64_t next_due_us = 0;
        bool periodic = false;
        bool active = false;
    };

    static void yield_if_available() {
#if defined(ARDUINO)
        yield();
#endif
    }

    IClock& clock_;
    FixedVector<TaskSlot, 32> tasks_;
    bool exit_requested_ = false;
};

} // namespace pypilot_event_loop
