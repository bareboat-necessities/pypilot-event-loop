#include <cassert>
#include <cstring>
#include <unistd.h>

#include "async_event_loop.hpp"

int main() {
    async_event_loop::NativeUdpDatagramStream a;
    async_event_loop::NativeUdpDatagramStream b;

    async_event_loop::IByteStream& byte_view = a;
    assert(byte_view.is_datagram());

    assert(a.bind(0));
    assert(b.bind(0));

    // Linux UDP bind(0) currently does not expose the chosen port through the
    // portable interface, so use two explicit local high ports for peer tests.
    a.close();
    b.close();
    assert(a.bind(23001));
    assert(b.bind(23002));

    async_event_loop::UdpEndpoint endpoint_b{};
    std::strncpy(endpoint_b.host, "127.0.0.1", sizeof(endpoint_b.host) - 1);
    endpoint_b.port = 23002;

    const uint8_t payload[] = {'u', 'd', 'p'};
    assert(a.send_to(payload, sizeof(payload), endpoint_b) == static_cast<int>(sizeof(payload)));

    uint8_t rx[16]{};
    async_event_loop::UdpEndpoint source{};
    int n = 0;
    for (int i = 0; i < 100; ++i) {
        n = b.recv_from(rx, sizeof(rx), &source);
        if (n > 0) break;
        usleep(1000);
    }

    assert(n == static_cast<int>(sizeof(payload)));
    assert(std::memcmp(rx, payload, sizeof(payload)) == 0);
    assert(source.port == 23001);
    assert(std::strcmp(source.host, "127.0.0.1") == 0);

    assert(b.set_remote("127.0.0.1", 23001));
    const uint8_t reply[] = {'o', 'k'};
    assert(b.write(reply, sizeof(reply)) == static_cast<int>(sizeof(reply)));

    async_event_loop::UdpEndpoint reply_source{};
    n = 0;
    for (int i = 0; i < 100; ++i) {
        n = a.recv_from(rx, sizeof(rx), &reply_source);
        if (n > 0) break;
        usleep(1000);
    }

    assert(n == static_cast<int>(sizeof(reply)));
    assert(std::memcmp(rx, reply, sizeof(reply)) == 0);
    assert(reply_source.port == 23002);

    a.close();
    b.close();
    return 0;
}
