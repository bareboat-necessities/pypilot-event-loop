#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdlib>
#include <iostream>
#endif

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;

#if defined(ARDUINO)
NativeSerialStream port(Serial);
#else
NativeSerialStream port;
#endif

static size_t text_len(const char* s) {
    size_t n = 0;
    while (s && s[n] != '\0') {
        ++n;
    }
    return n;
}

static void write_text(const char* s) {
    port.write(reinterpret_cast<const uint8_t*>(s), text_len(s));
}

LineProtocolReader<128> lines(port, LineProtocolOptions{}, [](LineView line) {
    write_text("line: ");
    if (line.data && line.size > 0) {
        port.write(reinterpret_cast<const uint8_t*>(line.data), line.size);
    }
    write_text("\n");
});

#if defined(ARDUINO)
void setup_example() {
    Serial.begin(115200);
    event_loop.on_bytes_ready(port, []() {
        lines.poll(event_loop.clock().micros());
    });
    write_text("line reader ready\n");
}
#else
bool setup_example(const char* device, unsigned long baud) {
    if (!port.open(device, baud)) {
        std::cerr << "failed to open port" << std::endl;
        return false;
    }
    event_loop.on_bytes_ready(port, []() {
        lines.poll(event_loop.clock().micros());
    });
    write_text("line reader ready\n");
    return true;
}
#endif

void loop_example() {
    event_loop.tick();
}

#ifdef ARDUINO
void setup() { setup_example(); }
void loop() { loop_example(); }
#else
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: serial_line_protocol_example DEVICE [BAUD]" << std::endl;
        return 2;
    }
    const unsigned long baud = argc > 2 ? std::strtoul(argv[2], nullptr, 10) : 115200UL;
    if (!setup_example(argv[1], baud)) {
        return 1;
    }
    event_loop.run_forever();
    return 0;
}
#endif
