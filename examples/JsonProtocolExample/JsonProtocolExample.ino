#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <async_event_loop.hpp>

using namespace async_event_loop;

template<size_t Capacity>
class JsonInputStream final : public IByteStream {
public:
    int read(uint8_t* dst, size_t max_len) override {
        if (!dst || max_len == 0) {
            return 0;
        }
        size_t n = 0;
        while (n < max_len && count_ > 0) {
            dst[n++] = buffer_[head_];
            head_ = (head_ + 1) % Capacity;
            --count_;
        }
        return static_cast<int>(n);
    }

    int write(const uint8_t* src, size_t len) override {
        if (!src || len == 0) {
            return 0;
        }
        size_t n = 0;
        while (n < len && count_ < Capacity) {
            buffer_[tail_] = src[n++];
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
        }
        return static_cast<int>(n);
    }

    bool readable() const override { return count_ > 0; }
    bool writable() const override { return count_ < Capacity; }
    bool valid() const override { return true; }

private:
    uint8_t buffer_[Capacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

EventLoop<> event_loop;
JsonInputStream<256> stream;
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
