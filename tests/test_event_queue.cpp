#include <cassert>
#include "pypilot_event_loop.hpp"

int main() {
    using namespace pypilot_event_loop;

    {
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
        assert(q.dropped() == 1);
        assert(q.overflowed());

        Event out;
        assert(q.pop(out));
        assert(out.source_id == 1);
        assert(out.sequence == 1);
        assert(q.pop(out));
        assert(out.source_id == 2);
        assert(out.sequence == 2);
        assert(q.push(e1));
        assert(q.pop(out));
        assert(out.source_id == 3);
        assert(out.sequence == 3);
        assert(q.pop(out));
        assert(out.source_id == 1);
        assert(out.sequence == 5);
        assert(!q.pop(out));
        assert(q.stats().popped == 4);
    }

    {
        BoundedEventQueue<2, EventQueueOverflowPolicy::DropOldest> q;
        Event a;
        a.type = EventType::User;
        a.source_id = 1;
        Event b = a;
        b.source_id = 2;
        Event c = a;
        c.source_id = 3;

        assert(q.push(a));
        assert(q.push(b));
        assert(q.push(c));
        assert(q.dropped() == 1);

        Event out;
        assert(q.pop(out));
        assert(out.source_id == 2);
        assert(q.pop(out));
        assert(out.source_id == 3);
        assert((out.flags & EventFlagOverflow) != 0);
    }

    {
        BoundedEventQueue<2, EventQueueOverflowPolicy::CoalesceBySource> q;
        Event a;
        a.type = EventType::User;
        a.source_id = 9;
        Event b = a;
        b.time_us = 10;
        Event c;
        c.type = EventType::Timer;
        c.source_id = 8;

        assert(q.push(a));
        assert(q.push(c));
        assert(q.push(b));
        assert(q.stats().coalesced == 1);

        Event out;
        assert(q.pop(out));
        assert(out.source_id == 9);
        assert(out.time_us == 10);
        assert((out.flags & EventFlagCoalesced) != 0);
    }

    return 0;
}
