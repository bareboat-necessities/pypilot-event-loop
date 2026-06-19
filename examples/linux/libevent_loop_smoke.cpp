#include <iostream>

#include "pypilot_event_loop.hpp"

class PrintTask final : public pypilot_event_loop::IRuntimeTask {
public:
    explicit PrintTask(pypilot_event_loop::NativeScheduler& scheduler) : scheduler_(scheduler) {}

    void poll(uint64_t now_us) override {
        std::cout << "tick " << count_ << " at " << now_us << " us" << std::endl;
        if (++count_ >= 3) {
            scheduler_.request_exit();
        }
    }

private:
    pypilot_event_loop::NativeScheduler& scheduler_;
    int count_ = 0;
};

int main() {
    pypilot_event_loop::NativeClock clock;
    pypilot_event_loop::NativeScheduler scheduler(clock);
    if (!scheduler.valid()) {
        return 1;
    }

    PrintTask task(scheduler);
    if (!scheduler.add_periodic(task, 100000)) {
        return 2;
    }

    scheduler.run_forever();
    return 0;
}
