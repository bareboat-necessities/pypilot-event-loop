#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

static size_t payload_size_from_header(const uint8_t* header, size_t header_size) {
    (void)header_size;
    return header[0];
}

EventLoop<> event_loop;
StaticByteStream<256> stream;
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

PYPILOT_EVENT_LOOP_EXAMPLE_MAIN(setup_example, loop_example)
