#include <cassert>
#include "support/memory_stream.hpp"

int main() {
    using async_event_loop_test::MemoryByteStream;

    MemoryByteStream<4> stream;
    assert(stream.valid());
    assert(!stream.readable());
    assert(stream.writable());

    const uint8_t in[5] = {1, 2, 3, 4, 5};
    assert(stream.write(in, 5) == 4);
    assert(stream.readable());
    assert(!stream.writable());

    uint8_t out[3] = {0, 0, 0};
    assert(stream.read(out, 3) == 3);
    assert(out[0] == 1);
    assert(out[1] == 2);
    assert(out[2] == 3);

    const uint8_t more[2] = {6, 7};
    assert(stream.write(more, 2) == 2);

    uint8_t out2[4] = {0, 0, 0, 0};
    assert(stream.read(out2, 4) == 3);
    assert(out2[0] == 4);
    assert(out2[1] == 6);
    assert(out2[2] == 7);

    return 0;
}
