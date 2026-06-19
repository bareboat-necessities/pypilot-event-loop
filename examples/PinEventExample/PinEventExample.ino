#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;
StaticPinEventSource<4> pin_source;
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

PYPILOT_EVENT_LOOP_EXAMPLE_MAIN(setup_example, loop_example)
