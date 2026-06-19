#include <cassert>
#include "pypilot_event_loop.hpp"

int main() {
    using namespace pypilot_event_loop;

    SpscEventQueue<2> q;
    assert(q.empty());
    assert(!q.full());

    Event a;
    a.type = EventType::PinEdge;
    a.source_id = 1;
    Event b;
    b.type = EventType::I2cReady;
    b.source_id = 2;
    Event c;
    c.type = EventType::User;
    c.source_id = 3;

    assert(q.push_from_producer(a));
    assert(q.push_from_producer(b));
    assert(q.full());
    assert(!q.push_from_producer(c));
    assert(q.dropped() == 1);

    Event out;
    assert(q.pop(out));
    assert(out.source_id == 1);
    assert(out.sequence == 1);
    assert((out.flags & EventFlagFromIsr) != 0);

    assert(q.pop(out));
    assert(out.source_id == 2);
    assert(out.sequence == 2);
    assert(!q.pop(out));

    assert(q.push_from_producer(c));
    assert(q.pop(out));
    assert(out.source_id == 3);
    assert(out.sequence == 3);

    return 0;
}
