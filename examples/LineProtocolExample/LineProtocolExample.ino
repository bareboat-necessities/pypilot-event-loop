#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;
StaticByteStream<256> stream;
uint32_t line_count = 0;

LineProtocolReader<128> lines(stream, LineProtocolOptions{}, [](LineView line) {
    (void)line;
    ++line_count;
});

void setup_example() {
    event_loop.on_bytes_ready(stream, []() {
        lines.poll(event_loop.clock().micros());
    });

    event_loop.on_delay(0, []() {
        const uint8_t msg[] = {'o','n','e','\n','t','w','o','\r','\n'};
        stream.write(msg, sizeof(msg));
    });
}

void loop_example() {
    event_loop.tick();
}

PYPILOT_EVENT_LOOP_EXAMPLE_MAIN(setup_example, loop_example)
