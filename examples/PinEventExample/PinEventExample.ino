#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <stddef.h>
#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;
uint32_t pin_events = 0;

template<size_t Capacity>
class LocalPinEventSource final : public IPinEventSource {
public:
    bool valid() const override { return true; }

    bool push(const PinEvent& event) {
        if (count_ >= Capacity) {
            return false;
        }
        events_[tail_] = event;
        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        return true;
    }

    bool read_event(PinEvent& event) override {
        if (count_ == 0) {
            return false;
        }
        event = events_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        return true;
    }

private:
    PinEvent events_[Capacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

LocalPinEventSource<4> pin_source;

void setup_example() {
    event_loop.on_pin_event(pin_source, [](const PinEvent& event) {
        (void)event;
        ++pin_events;
    }, 1);

    event_loop.on_delay(0, []() {
        PinEvent event;
        event.type = PinEventType::RisingEdge;
        event.timestamp_us = event_loop.clock().micros();
        event.source_id = 7;
        event.level = true;
        pin_source.push(event);
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
    return pin_events == 1 ? 0 : 1;
}
#endif
