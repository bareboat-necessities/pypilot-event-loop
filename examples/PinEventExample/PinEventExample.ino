#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <async_event_loop.hpp>

using namespace async_event_loop;

EventLoop<> event_loop;
uint32_t pin_events = 0;

MemoryPinEventSource<4> pin_source;

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
