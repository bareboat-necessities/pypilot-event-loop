#include "async_event_loop.hpp"

#include <cassert>

int main() {
    async_event_loop::EventLoop<64> loop;
    assert(loop.valid());

    async_event_loop::EventHandle handles[64];
    for (int i = 0; i < 64; ++i) {
        handles[i] = loop.on_repeat_us(1000, []() {});
        assert(handles[i].assigned());
    }

    const async_event_loop::EventHandle overflow = loop.on_repeat_us(1000, []() {});
    assert(!overflow.assigned());

    for (int i = 0; i < 64; ++i) {
        assert(loop.remove(handles[i]));
    }

    const async_event_loop::EventHandle reused = loop.on_repeat_us(1000, []() {});
    assert(reused.assigned());
    assert(loop.remove(reused));
    return 0;
}
