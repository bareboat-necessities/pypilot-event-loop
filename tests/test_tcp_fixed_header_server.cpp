#include <cassert>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pypilot_event_loop.hpp"

struct FixedHeaderHandler final : public pypilot_event_loop::ITcpServerHandler {
    int accepted = 0;
    int frames = 0;
    int closed = 0;

    void on_accept(pypilot_event_loop::ITcpConnection& connection,
                   const pypilot_event_loop::TcpPeerInfo& peer) override {
        (void)connection;
        (void)peer;
        ++accepted;
    }

    void on_data(pypilot_event_loop::ITcpConnection& connection) override {
        while (connection.input_size() >= 1) {
            uint8_t header = 0;
            if (!connection.read_exact(&header, 1)) {
                return;
            }
            uint8_t payload[32]{};
            assert(header <= sizeof(payload));
            if (!connection.read_exact(payload, header)) {
                return;
            }
            ++frames;
            connection.write(&header, 1);
            connection.write(payload, header);
        }
    }

    void on_close(pypilot_event_loop::ITcpConnection& connection) override {
        (void)connection;
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
    pypilot_event_loop::EventLoop<> event_loop;
    FixedHeaderHandler handler;
    pypilot_event_loop::NativeTcpServer server(event_loop.scheduler());

    pypilot_event_loop::TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 0;
    assert(server.listen(options, handler));

    int client = connect_client(server.port());
    for (int i = 0; i < 100 && handler.accepted == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.accepted == 1);

    const uint8_t message[] = {2, 'o', 'k', 3, 'y', 'e', 's'};
    assert(write(client, message, sizeof(message)) == static_cast<ssize_t>(sizeof(message)));

    for (int i = 0; i < 100 && handler.frames < 2; ++i) {
        event_loop.run_once();
    }
    assert(handler.frames == 2);

    uint8_t echo[8]{};
    for (int i = 0; i < 100; ++i) {
        event_loop.run_once();
        const ssize_t n = read(client, echo, sizeof(echo));
        if (n == static_cast<ssize_t>(sizeof(message))) {
            break;
        }
    }
    assert(echo[0] == 2);
    assert(echo[1] == 'o');
    assert(echo[2] == 'k');
    assert(echo[3] == 3);
    assert(echo[4] == 'y');
    assert(echo[5] == 'e');
    assert(echo[6] == 's');

    close(client);
    for (int i = 0; i < 100 && handler.closed == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.closed == 1);
    server.close();
    return 0;
}
