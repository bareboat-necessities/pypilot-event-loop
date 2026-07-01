#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "async_event_loop.hpp"

namespace {

class FakeTcpConnection final : public async_event_loop::ITcpConnection {
public:
    explicit FakeTcpConnection(const char* host) {
        std::snprintf(peer_.host, sizeof(peer_.host), "%s", host ? host : "");
    }

    bool valid() const override { return valid_; }
    void close() override { valid_ = false; }

    const async_event_loop::TcpPeerInfo& peer() const override { return peer_; }
    size_t input_size() const override { return 0; }
    size_t output_size() const override { return output_size_; }

    int read(uint8_t*, size_t) override { return 0; }

    int write(const uint8_t*, size_t len) override {
        output_size_ += len;
        return static_cast<int>(len);
    }

    bool peek(uint8_t*, size_t) override { return false; }
    bool read_exact(uint8_t*, size_t) override { return false; }
    bool read_line(char*, size_t, bool = true) override { return false; }

private:
    bool valid_ = true;
    size_t output_size_ = 0;
    async_event_loop::TcpPeerInfo peer_{};
};

} // namespace

int main() {
    async_event_loop::TcpConnectionRegistry<2> registry;
    assert(registry.empty());
    assert(registry.size() == 0);
    assert(registry.capacity() == 2);

    FakeTcpConnection a("a");
    FakeTcpConnection b("b");
    FakeTcpConnection c("c");

    assert(registry.add(a));
    assert(registry.add(b));
    assert(registry.full());
    assert(registry.size() == 2);
    assert(registry.at(0) == &a);
    assert(registry.at(1) == &b);
    assert(registry.contains(a));
    assert(registry.contains(b));
    assert(registry.index_of(a) == 0);
    assert(registry.index_of(b) == 1);

    assert(registry.add(a));
    assert(registry.size() == 2);
    assert(!registry.add(c));

    int visited = 0;
    registry.for_each([&](async_event_loop::ITcpConnection& connection) {
        const uint8_t byte = 0x42;
        assert(connection.write(&byte, 1) == 1);
        ++visited;
    });
    assert(visited == 2);
    assert(a.output_size() == 1);
    assert(b.output_size() == 1);

    assert(registry.remove(a));
    assert(!registry.contains(a));
    assert(registry.at(0) == nullptr);
    assert(registry.at(1) == &b);
    assert(registry.size() == 1);

    assert(registry.add(c));
    assert(registry.at(0) == &c);
    assert(registry.size() == 2);

    c.close();
    registry.prune_invalid();
    assert(registry.at(0) == nullptr);
    assert(registry.at(1) == &b);
    assert(registry.size() == 1);

    registry.clear();
    assert(registry.empty());
    assert(registry.at(0) == nullptr);
    assert(registry.at(1) == nullptr);

    return 0;
}
