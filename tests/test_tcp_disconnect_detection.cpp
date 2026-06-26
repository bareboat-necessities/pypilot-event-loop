#include <cassert>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "async_event_loop.hpp"

struct DisconnectHandler final : public async_event_loop::ITcpServerHandler {
    int accepted = 0;
    int closed = 0;
    int errors = 0;

    void on_accept(async_event_loop::ITcpConnection& connection,
                   const async_event_loop::TcpPeerInfo& peer) override {
        (void)connection;
        (void)peer;
        ++accepted;
    }

    void on_close(async_event_loop::ITcpConnection& connection) override {
        (void)connection;
        ++closed;
    }

    void on_error(async_event_loop::ITcpConnection& connection, int error_code) override {
        (void)connection;
        (void)error_code;
        ++errors;
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

int main() {
    async_event_loop::EventLoop<> event_loop;
    DisconnectHandler handler;
    async_event_loop::NativeTcpServer server(event_loop.scheduler());

    async_event_loop::TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 0;
    options.reuse_address = true;
    assert(server.listen(options, handler));

    int client = connect_client(server.port());
    for (int i = 0; i < 100 && handler.accepted == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.accepted == 1);
    assert(server.connection_count() == 1);

    close(client);
    for (int i = 0; i < 100 && handler.closed == 0; ++i) {
        event_loop.run_once();
    }

    assert(handler.closed == 1);
    assert(handler.errors == 0);
    assert(server.connection_count() == 0);
    server.close();
    return 0;
}
