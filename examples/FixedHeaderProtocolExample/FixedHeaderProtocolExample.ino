#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <pypilot_event_loop.hpp>
#include "../support/example_byte_stream.hpp"

using namespace pypilot_event_loop;
using pypilot_event_loop_examples::ExampleByteStream;

static size_t payload_size_from_header(const uint8_t* header, size_t header_size) {
    (void)header_size;
    return header[0];
}

EventLoop<> event_loop;
ExampleByteStream<256> stream;
uint32_t frame_count = 0;

HeaderPayloadProtocolReader<64> frames(
    stream,
    HeaderPayloadProtocolOptions{1, 16, payload_size_from_header},
    [](FrameView frame) {
        (void)frame;
        ++frame_count;
    });

void setup_example() {
    event_loop.on_bytes_ready(stream, []() {
        frames.poll(event_loop.clock().micros());
    });

    event_loop.on_delay(0, []() {
        const uint8_t msg[] = {2, 'o', 'k', 3, 'y', 'e', 's'};
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
    return frame_count == 2 ? 0 : 1;
}
#endif
