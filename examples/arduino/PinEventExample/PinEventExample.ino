#include <Arduino.h>
#include <pypilot_event_loop.hpp>
#include <pypilot_event_loop_arduino/arduino_pin_event_source.hpp>

pypilot_event_loop::EventLoop<> event_loop;
pypilot_event_loop::ArduinoDigitalPinEventSource pin_event(2, pypilot_event_loop::PinEventType::Change, INPUT_PULLUP);
uint32_t pin_event_count = 0;

void setup() {
    Serial.begin(115200);
    pin_event.begin();

    event_loop.on_pin_event(pin_event, [](const pypilot_event_loop::PinEvent& event) {
        pin_event_count++;
        (void)event;
    });
}

void loop() {
    event_loop.tick();
}
