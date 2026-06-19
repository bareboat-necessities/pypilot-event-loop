#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <pypilot_event_loop.hpp>
#include "../support/example_pin_event_source.hpp"

using namespace pypilot_event_loop;
using pypilot_event_loop_examples::ExamplePinEventSource;

EventLoop<> event_loop;
ExamplePinEventSource<4> pin_source;
uint32_t pin_events = 0;

void setup_example() {
    event_loop.on_pin_event(pin_source, [](const PinEvent& event) {
        (void)event;
        ++pin_events;
    }, 1);

    event_loop.on_delay(0, []() {
        PinEvent event;
        event.type = PinEventType::RisingEdge;
        event.timestamp_us = event_loop.clock().micros();
        event.source_id = 7;
        event.level = true;
        pin_source.push(event);
    });
}

void loop_example() {
    event_loop.tick();
}

#ifdef ARDUINO
void setup() { setup_example(); }
void loop() { loop_example(); }
#else
int main() {
    setup_example();
    for (int i = 0; i < 1000; ++i) {
        loop_example();
    }
    return pin_events == 1 ? 0 : 1;
}
#endif
