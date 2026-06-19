#include <cstring>
#include <iostream>

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

struct LineServerHandler final : public ITcpServerHandler {
    EventLoop<>* loop = nullptr;
    NativeTcpServer* server = nullptr;

    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)connection;
        std::cout << "accepted " << peer.host << ":" << peer.port << std::endl;
    }

    void on_data(ITcpConnection& connection) override {
        char line[256];
        while (connection.read_line(line, sizeof(line))) {
            std::cout << "line: " << line << std::endl;
            if (std::strcmp(line, "shutdown") == 0) {
                if (server) {
                    server->close();
                }
                if (loop) {
                    loop->request_exit();
                }
                return;
            }
            connection.write(reinterpret_cast<const uint8_t*>(line), std::strlen(line));
            const uint8_t newline = '\n';
            connection.write(&newline, 1);
        }
    }

    void on_close(ITcpConnection& connection) override {
        std::cout << "closed " << connection.peer().host << ":" << connection.peer().port << std::endl;
    }

    void on_error(ITcpConnection& connection, int error_code) override {
        std::cout << "connection error " << error_code
                  << " from " << connection.peer().host << ":" << connection.peer().port << std::endl;
    }

    void on_listener_error(int error_code) override {
        std::cout << "listener error " << error_code << std::endl;
    }
};

int main() {
    EventLoop<> event_loop;
    if (!event_loop.valid()) {
        return 1;
    }

    NativeTcpServer server(event_loop.scheduler());
    LineServerHandler handler;
    handler.loop = &event_loop;
    handler.server = &server;

    TcpListenOptions options;
    options.host = "0.0.0.0";
    options.port = 20220;
    options.reuse_address = true;

    if (!server.listen(options, handler)) {
        return 2;
    }

    std::cout << "TCP line server listening on port " << server.port() << std::endl;
    std::cout << "send 'shutdown' to stop" << std::endl;
    event_loop.run_forever();
    return 0;
}
