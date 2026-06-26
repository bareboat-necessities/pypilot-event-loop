#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "async_event_loop.hpp"

struct TimeoutHandler final : public async_event_loop::ITcpServerHandler {
    async_event_loop::ITcpConnection* connection = nullptr;
    int accepted = 0;
    int data = 0;
    int errors = 0;
    int last_error = 0;

    void on_accept(async_event_loop::ITcpConnection& c,
                   const async_event_loop::TcpPeerInfo& peer) override {
        (void)peer;
        connection = &c;
        ++accepted;
        async_event_loop::TcpTimeoutOptions timeouts;
        timeouts.read_timeout_ms = 10;
        assert(c.set_timeouts(timeouts));
        async_event_loop::TcpWatermarkOptions watermarks;
        watermarks.read_low = 2;
        watermarks.read_high = 0;
        watermarks.write_low = 0;
        watermarks.write_high = 0;
        assert(c.set_watermarks(watermarks));
    }

    void on_data(async_event_loop::ITcpConnection& c) override {
        (void)c;
        ++data;
    }

    void on_error(async_event_loop::ITcpConnection& c, int error_code) override {
        (void)c;
        ++errors;
        last_error = error_code;
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
    TimeoutHandler handler;
    async_event_loop::NativeTcpServer server(event_loop.scheduler());
    async_event_loop::TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 0;
    assert(server.listen(options, handler));

    int client = connect_client(server.port());
    for (int i = 0; i < 100 && handler.accepted == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.connection != nullptr);

    const uint8_t one = 'x';
    assert(write(client, &one, 1) == 1);
    for (int i = 0; i < 100; ++i) {
        event_loop.run_once();
    }
    assert(handler.data == 0);

    const uint8_t second = 'y';
    assert(write(client, &second, 1) == 1);
    for (int i = 0; i < 100 && handler.data == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.data == 1);

    close(client);
    server.close();

    TimeoutHandler timeout_handler;
    async_event_loop::NativeTcpServer timeout_server(event_loop.scheduler());
    assert(timeout_server.listen(options, timeout_handler));
    int timeout_client = connect_client(timeout_server.port());
    for (int i = 0; i < 100 && timeout_handler.accepted == 0; ++i) {
        event_loop.run_once();
    }
    assert(timeout_handler.connection != nullptr);

    for (int i = 0; i < 1000 && timeout_handler.errors == 0; ++i) {
        event_loop.run_once();
        usleep(1000);
    }
    assert(timeout_handler.errors == 1);
    assert(timeout_handler.last_error != 0);

    close(timeout_client);
    timeout_server.close();
    return 0;
}
