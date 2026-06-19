#pragma once

#include <stddef.h>
#include "pypilot_event_loop/clock.hpp"
#include "pypilot_event_loop/scheduler.hpp"

namespace pypilot_event_loop_test {

class TestScheduler final : public pypilot_event_loop::IScheduler {
public:
    explicit TestScheduler(pypilot_event_loop::IClock& clock) : clock_(clock) {}

    bool valid() const override { return true; }

    bool add_periodic(pypilot_event_loop::IRuntimeTask& task, uint64_t period_us) override {
        if (period_us == 0 || count_ >= MaxTasks) {
            return false;
        }
        tasks_[count_++] = TaskSlot{&task, period_us, clock_.micros(), true, true};
        return true;
    }

    bool add_one_shot(pypilot_event_loop::IRuntimeTask& task, uint64_t due_us) override {
        if (count_ >= MaxTasks) {
            return false;
        }
        tasks_[count_++] = TaskSlot{&task, 0, due_us, false, true};
        return true;
    }

    void run_once() override {
        const uint64_t now_us = clock_.micros();
        for (size_t i = 0; i < count_; ++i) {
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
        }
    }

    void request_exit() override { exit_requested_ = true; }

private:
    static constexpr size_t MaxTasks = 64;

    struct TaskSlot {
        pypilot_event_loop::IRuntimeTask* task = nullptr;
        uint64_t period_us = 0;
        uint64_t next_due_us = 0;
        bool periodic = false;
        bool active = false;
    };

    pypilot_event_loop::IClock& clock_;
    TaskSlot tasks_[MaxTasks]{};
    size_t count_ = 0;
    bool exit_requested_ = false;
};

} // namespace pypilot_event_loop_test
