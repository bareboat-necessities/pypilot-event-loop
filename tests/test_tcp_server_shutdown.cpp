#include <cassert>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pypilot_event_loop.hpp"

struct ShutdownHandler final : public pypilot_event_loop::ITcpServerHandler {
    pypilot_event_loop::EventLoop<>* loop = nullptr;
    pypilot_event_loop::NativeTcpServer* server = nullptr;
    int accepted = 0;
    int shutdowns = 0;

    void on_accept(pypilot_event_loop::ITcpConnection& connection,
                   const pypilot_event_loop::TcpPeerInfo& peer) override {
        (void)connection;
        (void)peer;
        ++accepted;
    }

    void on_data(pypilot_event_loop::ITcpConnection& connection) override {
        char line[64];
        while (connection.read_line(line, sizeof(line))) {
            if (std::strcmp(line, "shutdown") == 0) {
                ++shutdowns;
                if (server) {
                    server->close();
                }
                if (loop) {
                    loop->request_exit();
                }
                return;
            }
        }
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
    pypilot_event_loop::EventLoop<> event_loop;
    pypilot_event_loop::NativeTcpServer server(event_loop.scheduler());
    ShutdownHandler handler;
    handler.loop = &event_loop;
    handler.server = &server;

    pypilot_event_loop::TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 0;
    assert(server.listen(options, handler));

    int client = connect_client(server.port());
    for (int i = 0; i < 100 && handler.accepted == 0; ++i) {
        event_loop.run_once();
    }
    assert(handler.accepted == 1);

    assert(write(client, "shutdown\n", 9) == 9);
    for (int i = 0; i < 100 && server.valid(); ++i) {
        event_loop.run_once();
    }

    assert(handler.shutdowns == 1);
    assert(!server.valid());
    close(client);
    return 0;
}
