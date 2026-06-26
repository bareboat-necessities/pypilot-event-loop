#include <cassert>
#include <stdint.h>

#include "async_event_loop/micros32_extender.hpp"

int main() {
    async_event_loop::Micros32Extender clock;

    assert(clock.update(100u) == 100ULL);
    assert(clock.update(150u) == 150ULL);

    clock.reset(0xfffffff0u);
    assert(clock.update(0xfffffff5u) == 0xfffffff5ULL);
    assert(clock.update(0x00000005u) == 0x100000005ULL);
    assert(clock.update(0x00000025u) == 0x100000025ULL);

    clock.reset(0xfffffffeu);
    assert(clock.update(0xffffffffu) == 0xffffffffULL);
    assert(clock.update(0x00000000u) == 0x100000000ULL);
    assert(clock.update(0x00000001u) == 0x100000001ULL);

    return 0;
}
