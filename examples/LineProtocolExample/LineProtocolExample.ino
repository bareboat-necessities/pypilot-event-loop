#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

template<size_t Capacity>
class ExampleByteStream final : public IByteStream {
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
ExampleByteStream<256> stream;
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
