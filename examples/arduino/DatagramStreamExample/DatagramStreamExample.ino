#include <Arduino.h>
#include <pypilot_event_loop.hpp>

pypilot_event_loop::EventLoop<> event_loop;
pypilot_event_loop::StaticDatagramStream<4, 32> stream;
uint32_t packets_read = 0;

void setup() {
    Serial.begin(115200);

    event_loop.on_readable(stream, []() {
        uint8_t buf[16];
        const int n = stream.recv(buf, sizeof(buf));
        if (n > 0) {
            packets_read++;
        }
    });

    event_loop.on_delay(0, []() {
        const uint8_t msg[] = {'d', 'a', 't', 'a'};
        stream.send(msg, sizeof(msg));
    });
}

void loop() {
    event_loop.tick();
}
