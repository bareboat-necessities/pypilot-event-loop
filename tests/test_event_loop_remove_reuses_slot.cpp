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
    assert(!event_loop.valid(second_handle));

    int third = 0;
    const pypilot_event_loop::EventHandle third_handle = event_loop.on_delay(0, [&]() {
        ++third;
    });
    assert(third_handle.assigned());
    assert(third_handle.slot == second_handle.slot);
    assert(third_handle.generation != second_handle.generation);

    event_loop.run_once();
    assert(third == 1);
    assert(!event_loop.valid(third_handle));
    return 0;
}
