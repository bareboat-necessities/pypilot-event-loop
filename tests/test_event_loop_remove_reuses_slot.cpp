#include <cassert>

#include "pypilot_event_loop.hpp"

int main() {
    pypilot_event_loop::EventLoop<1> event_loop;

    int first = 0;
    const pypilot_event_loop::EventHandle first_handle = event_loop.on_repeat(1000, [&]() {
        ++first;
    });
    assert(first_handle.assigned());
    assert(event_loop.valid(first_handle));
    assert(event_loop.remove(first_handle));
    assert(!event_loop.valid(first_handle));

    int second = 0;
    const pypilot_event_loop::EventHandle second_handle = event_loop.on_delay(0, [&]() {
        ++second;
    });
    assert(second_handle.assigned());
    assert(event_loop.valid(second_handle));
    assert(second_handle.slot == first_handle.slot);
    assert(second_handle.generation != first_handle.generation);

    event_loop.run_once();
    assert(first == 0);
    assert(second == 1);
    return 0;
}
