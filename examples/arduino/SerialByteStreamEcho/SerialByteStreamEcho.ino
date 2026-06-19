#include <Arduino.h>
#include <pypilot_event_loop.hpp>

pypilot_event_loop::EventLoop<> event_loop;
pypilot_event_loop::StaticByteStream<64> stream;
uint32_t bytes_read = 0;

void setup() {
    Serial.begin(115200);

    event_loop.on_delay(0, []() {
        const uint8_t msg[] = {'h', 'e', 'l', 'l', 'o'};
        stream.write(msg, sizeof(msg));
    });

    event_loop.on_repeat(1, []() {
        uint8_t buf[16];
        const int n = stream.read(buf, sizeof(buf));
        if (n > 0) {
            bytes_read += static_cast<uint32_t>(n);
        }
    });
}

void loop() {
    event_loop.tick();
}
