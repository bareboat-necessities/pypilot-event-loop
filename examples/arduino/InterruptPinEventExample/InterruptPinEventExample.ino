#include <Arduino.h>
#include <pypilot_event_loop.hpp>
#include <pypilot_event_loop_arduino/arduino_interrupt_pin_event_source.hpp>

constexpr uint8_t BUTTON_PIN = 2;

pypilot_event_loop::EventLoop<> event_loop;
pypilot_event_loop::ArduinoInterruptPinEventSource button_event(
    BUTTON_PIN,
    pypilot_event_loop::PinEventType::RisingEdge);

volatile bool level_hint = false;
uint32_t handled_events = 0;

void button_isr() {
    level_hint = digitalRead(BUTTON_PIN) == HIGH;
    button_event.notify_from_isr(level_hint);
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr, RISING);

    event_loop.on_pin_event(button_event, [](const pypilot_event_loop::PinEvent& event) {
        handled_events += button_event.last_info().count;
        (void)event;
    });
}

void loop() {
    event_loop.tick();
}
