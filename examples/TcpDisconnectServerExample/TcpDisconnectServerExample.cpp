#include <iostream>

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

struct DisconnectHandler final : public ITcpServerHandler {
    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        std::cout << "accepted " << peer.host << ":" << peer.port << std::endl;
        const char greeting[] = "connected; close the client to test disconnect detection\n";
        connection.write(reinterpret_cast<const uint8_t*>(greeting), sizeof(greeting) - 1);
    }

    void on_data(ITcpConnection& connection) override {
        uint8_t buffer[128];
        while (connection.input_size() > 0) {
            const int n = connection.read(buffer, sizeof(buffer));
            if (n <= 0) {
                return;
            }
            std::cout << "received " << n << " bytes" << std::endl;
        }
    }

    void on_close(ITcpConnection& connection) override {
        std::cout << "peer disconnected "
                  << connection.peer().host << ":" << connection.peer().port << std::endl;
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
    DisconnectHandler handler;

    TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 20222;
    options.reuse_address = true;

    if (!server.listen(options, handler)) {
        return 2;
    }

    std::cout << "TCP disconnect detection server listening on port " << server.port() << std::endl;
    event_loop.run_forever();
    return 0;
}
