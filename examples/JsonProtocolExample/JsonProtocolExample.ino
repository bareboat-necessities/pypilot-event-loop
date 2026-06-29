#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <async_event_loop.hpp>

using namespace async_event_loop;

EventLoop<> event_loop;
MemoryByteStream<256> stream;
uint32_t json_count = 0;
size_t json_bytes = 0;

JsonProtocolReader<128> json_messages(stream, JsonProtocolOptions{}, [](JsonView json) {
    ++json_count;
    json_bytes += json.size;
#if defined(ARDUINO)
    Serial.print("json: ");
    Serial.write(reinterpret_cast<const uint8_t*>(json.data), json.size);
    Serial.println();
#endif
});

void setup_example() {
#if defined(ARDUINO)
    Serial.begin(115200);
#endif

    event_loop.on_bytes_ready(stream, []() {
        json_messages.poll(event_loop.clock().micros());
    });

    event_loop.on_delay(0, []() {
        const char messages[] =
            " {\"ap\":{\"enabled\":true}}"
            " [1,2,{\"mode\":\"compass\"}]"
            " \"ready\""
            " null";
        stream.write(reinterpret_cast<const uint8_t*>(messages), sizeof(messages) - 1);
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
    return json_count == 4 && json_bytes > 0 ? 0 : 1;
}
#endif
