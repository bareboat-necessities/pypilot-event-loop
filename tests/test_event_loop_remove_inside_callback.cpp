#include <cassert>

#include "pypilot_event_loop.hpp"

int main() {
    pypilot_event_loop::EventLoop<1> event_loop;

    int count = 0;
    pypilot_event_loop::EventHandle handle;
    handle = event_loop.on_delay(0, [&]() {
        ++count;
        assert(event_loop.remove(handle));
    });

    assert(handle.assigned());
    assert(event_loop.valid(handle));

    for (int i = 0; i < 10 && count == 0; ++i) {
        event_loop.run_once();
    }

    assert(count == 1);
    assert(!event_loop.valid(handle));

    int second = 0;
    const pypilot_event_loop::EventHandle second_handle = event_loop.on_delay(0, [&]() {
        ++second;
    });
    assert(second_handle.assigned());
    assert(second_handle.slot == handle.slot);

    for (int i = 0; i < 10 && second == 0; ++i) {
        event_loop.run_once();
    }

    assert(second == 1);
    return 0;
}
