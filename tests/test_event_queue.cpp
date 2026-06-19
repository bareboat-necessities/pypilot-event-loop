#include <cassert>
#include "pypilot_event_loop.hpp"

int main() {
    using namespace pypilot_event_loop;

    EventQueue<3> q;
    assert(q.empty());
    assert(!q.full());

    Event e1;
    e1.type = EventType::Timer;
    e1.source_id = 1;
    Event e2;
    e2.type = EventType::User;
    e2.source_id = 2;
    Event e3;
    e3.type = EventType::Shutdown;
    e3.source_id = 3;

    assert(q.push(e1));
    assert(q.push(e2));
    assert(q.push(e3));
    assert(q.full());
    assert(!q.push(e1));

    Event out;
    assert(q.pop(out));
    assert(out.source_id == 1);
    assert(q.pop(out));
    assert(out.source_id == 2);
    assert(q.push(e1));
    assert(q.pop(out));
    assert(out.source_id == 3);
    assert(q.pop(out));
    assert(out.source_id == 1);
    assert(!q.pop(out));

    return 0;
}
