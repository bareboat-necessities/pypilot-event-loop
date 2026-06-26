#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "async_event_loop.hpp"

struct OutputHandler final : public async_event_loop::ITcpServerHandler {
    async_event_loop::ITcpConnection* connection = nullptr;
    int accepted = 0;
    int write_ready = 0;
    int closed = 0;

    void on_accept(async_event_loop::ITcpConnection& c,
                   const async_event_loop::TcpPeerInfo& peer) override {
        (void)peer;
        connection = &c;
        ++accepted;
    }

    void on_write_ready(async_event_loop::ITcpConnection& c) override {
        (void)c;
        ++write_ready;
    }

    void on_close(async_event_loop::ITcpConnection& c) override {
        (void)c;
        ++closed;
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
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

int main() {
    async_event_loop::EventLoop<> event_loop;
    OutputHandler handler;
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
    assert(handler.connection != nullptr);

    uint8_t block[4096];
    std::memset(block, 'x', sizeof(block));
    size_t queued = 0;
    for (int i = 0; i < 256; ++i) {
        const int n = handler.connection->write(block, sizeof(block));
        assert(n == static_cast<int>(sizeof(block)));
        queued += sizeof(block);
        if (handler.connection->output_size() > 0) {
            break;
        }
    }
    assert(queued > 0);
    assert(handler.connection->output_size() > 0);

    uint8_t sink[8192];
    size_t drained = 0;
    for (int i = 0; i < 500 && drained < queued; ++i) {
        event_loop.run_once();
        const ssize_t n = read(client, sink, sizeof(sink));
        if (n > 0) {
            drained += static_cast<size_t>(n);
        }
    }
    assert(drained > 0);

    close(client);
    for (int i = 0; i < 100 && handler.closed == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.closed == 1);
    server.close();
    return 0;
}
