#include <Arduino.h>
#include <pypilot_event_loop.hpp>
#include <pypilot_event_loop_arduino/arduino_serial_stream.hpp>

pypilot_event_loop::EventLoop<> event_loop;
pypilot_event_loop::ArduinoSerialStream serial_stream(Serial);

void setup() {
    Serial.begin(115200);

    event_loop.onRepeat(10, []() {
        uint8_t buf[32];
        const int n = serial_stream.read(buf, sizeof(buf));
        if (n > 0) {
            serial_stream.write(buf, static_cast<size_t>(n));
        }
    });
}

void loop() {
    event_loop.tick();
}
