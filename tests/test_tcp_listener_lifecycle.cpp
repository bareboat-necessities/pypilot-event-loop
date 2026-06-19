#include <cassert>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pypilot_event_loop.hpp"

struct LifecycleHandler final : public pypilot_event_loop::ITcpServerHandler {
    int accepted = 0;
    int listener_errors = 0;

    void on_accept(pypilot_event_loop::ITcpConnection& connection,
                   const pypilot_event_loop::TcpPeerInfo& peer) override {
        (void)connection;
        (void)peer;
        ++accepted;
    }

    void on_listener_error(int error_code) override {
        (void)error_code;
        ++listener_errors;
    }
};

static int connect_client(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(connect(fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0);
    return fd;
}

static bool can_connect(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const bool ok = connect(fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0;
    close(fd);
    return ok;
}

int main() {
    pypilot_event_loop::EventLoop<> event_loop;
    LifecycleHandler handler;
    pypilot_event_loop::NativeTcpServer server(event_loop.scheduler());

    pypilot_event_loop::TcpListenOptions bad_options;
    bad_options.host = "not-an-ip-address";
    bad_options.port = 0;
    assert(!server.listen(bad_options, handler));
    assert(!server.valid());

    pypilot_event_loop::TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 0;
    options.reuse_address = true;
    assert(server.listen(options, handler));
    assert(server.valid());
    assert(server.port() != 0);

    const uint16_t port = server.port();
    int client = connect_client(port);
    for (int i = 0; i < 100 && handler.accepted == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.accepted == 1);
    assert(server.connection_count() == 1);

    server.close();
    assert(!server.valid());
    assert(server.connection_count() == 0);
    assert(!can_connect(port));
    assert(handler.listener_errors == 0);

    close(client);
    return 0;
}
