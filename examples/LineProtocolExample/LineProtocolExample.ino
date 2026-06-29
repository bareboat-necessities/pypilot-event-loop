#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <async_event_loop.hpp>

using namespace async_event_loop;

EventLoop<> event_loop;
MemoryByteStream<256> stream;
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

#ifdef ARDUINO
void setup() { setup_example(); }
void loop() { loop_example(); }
#else
int main() {
    setup_example();
    for (int i = 0; i < 1000; ++i) {
        loop_example();
    }
    return line_count == 2 ? 0 : 1;
}
#endif
