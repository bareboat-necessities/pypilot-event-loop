#include <Arduino.h>
#include <pypilot_event_loop.hpp>

pypilot_event_loop::EventLoop<> event_loop;
uint32_t tick_count = 0;

void setup() {
    Serial.begin(115200);

    event_loop.onRepeat(100, []() {
        tick_count++;
    });
}

void loop() {
    event_loop.tick();
}
