#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "async_event_loop.hpp"

struct CloseClientHandler final : public async_event_loop::ITcpServerHandler {
    int accepted = 0;

    void on_accept(async_event_loop::ITcpConnection& connection,
                   const async_event_loop::TcpPeerInfo& peer) override {
        (void)connection;
        (void)peer;
        ++accepted;
    }
};

static int connect_nonblocking_client(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(connect(fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

static bool wait_for_eof(async_event_loop::EventLoop<>& event_loop, int fd) {
    uint8_t byte = 0;
    for (int i = 0; i < 100; ++i) {
        event_loop.run_once();
        const ssize_t n = read(fd, &byte, 1);
        if (n == 0) {
            return true;
        }
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            return false;
        }
    }
    return false;
}

int main() {
    async_event_loop::EventLoop<> event_loop;
    CloseClientHandler handler;
    async_event_loop::NativeTcpServer server(event_loop.scheduler());

    async_event_loop::TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 0;
    options.reuse_address = true;
    assert(server.listen(options, handler));

    int client_a = connect_nonblocking_client(server.port());
    int client_b = connect_nonblocking_client(server.port());

    for (int i = 0; i < 100 && handler.accepted < 2; ++i) {
        event_loop.run_once();
    }
    assert(handler.accepted == 2);
    assert(server.connection_count() == 2);

    server.close();
    assert(!server.valid());
    assert(server.connection_count() == 0);
    assert(wait_for_eof(event_loop, client_a));
    assert(wait_for_eof(event_loop, client_b));

    close(client_a);
    close(client_b);
    return 0;
}
