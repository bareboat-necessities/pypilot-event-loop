#include <cassert>
#include "pypilot_event_loop/pin_event.hpp"

class TestPin final : public pypilot_event_loop::IPinInput {
public:
    bool valid() const override { return true; }
    bool read() const override { return level; }
    uint32_t id() const override { return 7; }

    bool level = false;
};

class CountTask final : public pypilot_event_loop::IRuntimeTask {
public:
    void poll(uint64_t now_us) override {
        count++;
        last_us = now_us;
    }

    int count = 0;
    uint64_t last_us = 0;
};

int main() {
    using namespace pypilot_event_loop;

    TestPin pin;
    CountTask task;
    PinEventTask edge_task(pin, task, PinEventType::RisingEdge);

    edge_task.poll(0);
    assert(task.count == 0);

    pin.level = true;
    edge_task.poll(10);
    assert(task.count == 1);

    edge_task.poll(20);
    assert(task.count == 1);

    pin.level = false;
    edge_task.poll(30);
    assert(task.count == 1);

    pin.level = true;
    edge_task.poll(40);
    assert(task.count == 2);

    int lambda_count = 0;
    LambdaPinEventTask<> lambda_task(pin, PinEventType::FallingEdge, [&lambda_count]() {
        lambda_count++;
    });

    lambda_task.poll(50);
    pin.level = false;
    lambda_task.poll(60);
    assert(lambda_count == 1);

    return 0;
}
