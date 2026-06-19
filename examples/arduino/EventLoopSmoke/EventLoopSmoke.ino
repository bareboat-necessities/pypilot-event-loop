#include <Arduino.h>
#include <pypilot_event_loop.hpp>

class SmokeTask final : public pypilot_event_loop::IRuntimeTask {
public:
    void poll(uint64_t now_us) override {
        last_us = now_us;
        count++;
    }

    uint64_t last_us = 0;
    uint32_t count = 0;
};

pypilot_event_loop::NativeClock event_clock;
pypilot_event_loop::NativeScheduler event_scheduler(event_clock);
SmokeTask smoke_task;

void setup() {
    Serial.begin(115200);
    event_scheduler.add_periodic(smoke_task, 100000);
}

void loop() {
    event_scheduler.run_once();
}
