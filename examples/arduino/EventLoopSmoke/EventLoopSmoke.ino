#include <Arduino.h>
#include <pypilot_event_loop.hpp>
#include <pypilot_event_loop_arduino/arduino_clock.hpp>
#include <pypilot_event_loop_arduino/arduino_loop.hpp>
#include <pypilot_event_loop_arduino/arduino_serial_stream.hpp>

using namespace pypilot_event_loop;

class SmokeTask final : public IRuntimeTask {
public:
    void poll(uint64_t now_us) override {
        last_us = now_us;
        count++;
    }

    uint64_t last_us = 0;
    uint32_t count = 0;
};

ArduinoClock clock_;
ArduinoLoop scheduler_(clock_);
SmokeTask smoke_task_;

void setup() {
    Serial.begin(115200);
    scheduler_.add_periodic(smoke_task_, 100000);
}

void loop() {
    scheduler_.tick();

    ArduinoSerialStream serial_stream(Serial);
    const uint8_t msg[] = {'o', 'k', '\n'};
    if (smoke_task_.count == 1) {
        serial_stream.write(msg, sizeof(msg));
    }
}
