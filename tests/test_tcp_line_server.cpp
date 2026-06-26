#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "async_event_loop.hpp"

struct LineHandler final : public async_event_loop::ITcpServerHandler {
    int accepted = 0;
    int lines = 0;
    int closed = 0;
    int errors = 0;
    char captured[4][32]{};

    void on_accept(async_event_loop::ITcpConnection& connection,
                   const async_event_loop::TcpPeerInfo& peer) override {
        (void)connection;
        (void)peer;
        ++accepted;
    }

    void on_data(async_event_loop::ITcpConnection& connection) override {
        char line[64];
        while (connection.read_line(line, sizeof(line))) {
            assert(lines < 4);
            std::strncpy(captured[lines], line, sizeof(captured[lines]) - 1);
            ++lines;
            connection.write(reinterpret_cast<const uint8_t*>(line), std::strlen(line));
            const uint8_t nl = '\n';
            connection.write(&nl, 1);
        }
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
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

static void pump(async_event_loop::EventLoop<>& event_loop, int iterations = 100) {
    for (int i = 0; i < iterations; ++i) {
        event_loop.run_once();
    }
}

int main() {
    async_event_loop::EventLoop<> event_loop;
    assert(event_loop.valid());

    LineHandler handler;
    async_event_loop::NativeTcpServer server(event_loop.scheduler());
    async_event_loop::TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 0;
    options.reuse_address = true;

    assert(server.listen(options, handler));
    assert(server.valid());
    assert(server.port() != 0);

    int client = connect_client(server.port());

    for (int i = 0; i < 100 && handler.accepted == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.accepted == 1);
    assert(server.connection_count() == 1);

    assert(write(client, "hel", 3) == 3);
    pump(event_loop);
    assert(handler.lines == 0);

    assert(write(client, "lo\r\nnext\n", 9) == 9);
    for (int i = 0; i < 100 && handler.lines < 2; ++i) {
        event_loop.run_once();
    }
    assert(handler.lines == 2);
    assert(std::strcmp(handler.captured[0], "hello") == 0);
    assert(std::strcmp(handler.captured[1], "next") == 0);

    char echo[32]{};
    for (int i = 0; i < 100; ++i) {
        event_loop.run_once();
        const ssize_t n = read(client, echo, sizeof(echo) - 1);
        if (n > 0) {
            echo[n] = '\0';
            break;
        }
    }
    assert(std::strstr(echo, "hello\n") != nullptr);

    assert(write(client, "a\nb\n", 4) == 4);
    for (int i = 0; i < 100 && handler.lines < 4; ++i) {
        event_loop.run_once();
    }
    assert(handler.lines == 4);
    assert(std::strcmp(handler.captured[2], "a") == 0);
    assert(std::strcmp(handler.captured[3], "b") == 0);

    close(client);
    server.close();
    assert(!server.valid());
    return 0;
}
