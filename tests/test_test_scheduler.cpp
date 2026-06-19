#include <cassert>
#include "pypilot_event_loop.hpp"
#include "support/test_clock.hpp"
#include "support/test_scheduler.hpp"

class CountTask final : public pypilot_event_loop::IRuntimeTask {
public:
    void poll(uint64_t now_us) override {
        last_us = now_us;
        count++;
    }

    uint64_t last_us = 0;
    int count = 0;
};

int main() {
    using pypilot_event_loop_test::TestClock;
    using pypilot_event_loop_test::TestScheduler;

    TestClock clock;
    TestScheduler loop(clock);
    CountTask periodic;
    CountTask once;

    assert(loop.add_periodic(periodic, 100));
    assert(loop.add_one_shot(once, 250));

    loop.run_once();
    assert(periodic.count == 1);
    assert(once.count == 0);

    clock.advance_us(99);
    loop.run_once();
    assert(periodic.count == 1);

    clock.advance_us(1);
    loop.run_once();
    assert(periodic.count == 2);

    clock.set_us(250);
    loop.run_once();
    assert(once.count == 1);

    loop.run_once();
    assert(once.count == 1);

    return 0;
}
