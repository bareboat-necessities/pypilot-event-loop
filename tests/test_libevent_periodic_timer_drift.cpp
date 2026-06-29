#include <cassert>
#include <stdint.h>

#include "async_event_loop_linux/libevent_loop.hpp"

int main() {
    using async_event_loop::detail::periodic_timer_next_delay_us;

    {
        uint64_t next_due_us = 1000000ULL;
        const uint64_t delay_us = periodic_timer_next_delay_us(next_due_us, 1000ULL, 1000000ULL);
        assert(delay_us == 1000ULL);
        assert(next_due_us == 1001000ULL);
    }

    {
        uint64_t next_due_us = 1000000ULL;
        const uint64_t delay_us = periodic_timer_next_delay_us(next_due_us, 1000ULL, 1000050ULL);
        assert(delay_us == 950ULL);
        assert(next_due_us == 1001000ULL);
    }

    {
        uint64_t next_due_us = 1000000ULL;
        const uint64_t delay_us = periodic_timer_next_delay_us(next_due_us, 1000ULL, 1001500ULL);
        assert(delay_us == 0ULL);
        assert(next_due_us == 1001000ULL);
    }

    {
        uint64_t next_due_us = 1001000ULL;
        const uint64_t delay_us = periodic_timer_next_delay_us(next_due_us, 1000ULL, 1001500ULL);
        assert(delay_us == 500ULL);
        assert(next_due_us == 1002000ULL);
    }

    return 0;
}
