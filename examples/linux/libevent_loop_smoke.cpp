#include <iostream>

#include "pypilot_event_loop_linux/libevent_loop.hpp"
#include "pypilot_event_loop_linux/monotonic_clock.hpp"

class PrintTask final : public pypilot_event_loop::IRuntimeTask {
public:
    explicit PrintTask(pypilot_event_loop::LinuxLibeventLoop& loop) : loop_(loop) {}

    void poll(uint64_t now_us) override {
        std::cout << "tick " << count_ << " at " << now_us << " us" << std::endl;
        if (++count_ >= 3) {
            loop_.request_exit();
        }
    }

private:
    pypilot_event_loop::LinuxLibeventLoop& loop_;
    int count_ = 0;
};

int main() {
    pypilot_event_loop::LinuxMonotonicClock clock;
    pypilot_event_loop::LinuxLibeventLoop loop(clock);
    if (!loop.valid()) {
        return 1;
    }

    PrintTask task(loop);
    if (!loop.add_periodic(task, 100000)) {
        return 2;
    }

    loop.run_forever();
    return 0;
}
