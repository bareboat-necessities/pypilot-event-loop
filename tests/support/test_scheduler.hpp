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
        compact_inactive();
        if (period_us == 0 || count_ >= MaxTasks) {
            return false;
        }
        TaskSlot slot;
        slot.task = &task;
        slot.period_us = period_us;
        slot.next_due_us = clock_.micros();
        slot.periodic = true;
        slot.active = true;
        tasks_[count_++] = slot;
        return true;
    }

    bool add_one_shot(pypilot_event_loop::IRuntimeTask& task, uint64_t due_us) override {
        compact_inactive();
        if (count_ >= MaxTasks) {
            return false;
        }
        TaskSlot slot;
        slot.task = &task;
        slot.period_us = 0;
        slot.next_due_us = due_us;
        slot.periodic = false;
        slot.active = true;
        tasks_[count_++] = slot;
        return true;
    }

    bool remove(pypilot_event_loop::IRuntimeTask& task) override {
        bool removed = false;
        for (size_t i = 0; i < count_; ++i) {
            if (tasks_[i].task == &task) {
                tasks_[i].task = nullptr;
                tasks_[i].active = false;
                removed = true;
            }
        }
        if (!running_) {
            compact_inactive();
        }
        return removed;
    }

    void run_once() override {
        compact_inactive();
        running_ = true;
        const uint64_t now_us = clock_.micros();
        for (size_t i = 0; i < count_; ++i) {
            TaskSlot& slot = tasks_[i];
            if (!slot.active || !slot.task || now_us < slot.next_due_us) {
                continue;
            }
            slot.task->poll(now_us);
            if (!slot.active || !slot.task) {
                continue;
            }
            if (slot.periodic) {
                do {
                    slot.next_due_us += slot.period_us;
                } while (now_us >= slot.next_due_us);
            } else {
                slot.active = false;
                slot.task = nullptr;
            }
        }
        running_ = false;
        compact_inactive();
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

    void compact_inactive() {
        for (size_t i = 0; i < count_;) {
            if (!tasks_[i].active || !tasks_[i].task) {
                for (size_t j = i + 1; j < count_; ++j) {
                    tasks_[j - 1] = tasks_[j];
                }
                --count_;
            } else {
                ++i;
            }
        }
    }

    pypilot_event_loop::IClock& clock_;
    TaskSlot tasks_[MaxTasks]{};
    size_t count_ = 0;
    bool exit_requested_ = false;
    bool running_ = false;
};

} // namespace pypilot_event_loop_test
