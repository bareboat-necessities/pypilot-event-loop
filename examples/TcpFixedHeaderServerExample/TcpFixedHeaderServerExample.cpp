#include <iostream>

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

struct FixedHeaderServerHandler final : public ITcpServerHandler {
    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)connection;
        std::cout << "accepted " << peer.host << ":" << peer.port << std::endl;
    }

    void on_data(ITcpConnection& connection) override {
        while (connection.input_size() >= 1) {
            uint8_t length = 0;
            if (!connection.peek(&length, 1)) {
                return;
            }
            if (length > 64) {
                connection.close();
                return;
            }
            const size_t frame_size = static_cast<size_t>(1 + length);
            if (connection.input_size() < frame_size) {
                return;
            }
            uint8_t frame[65]{};
            if (!connection.read_exact(frame, frame_size)) {
                return;
            }
            connection.write(frame, frame_size);
        }
    }

    void on_close(ITcpConnection& connection) override {
        std::cout << "closed " << connection.peer().host << ":" << connection.peer().port << std::endl;
    }

    void on_error(ITcpConnection& connection, int error_code) override {
        std::cout << "connection error " << error_code
                  << " from " << connection.peer().host << ":" << connection.peer().port << std::endl;
    }
};

int main() {
    EventLoop<> event_loop;
    if (!event_loop.valid()) {
        return 1;
    }

    NativeTcpServer server(event_loop.scheduler());
    FixedHeaderServerHandler handler;

    TcpListenOptions options;
    options.host = "0.0.0.0";
    options.port = 20221;
    options.reuse_address = true;

    if (!server.listen(options, handler)) {
        return 2;
    }

    std::cout << "TCP fixed-header server listening on port " << server.port() << std::endl;
    std::cout << "message format: one length byte followed by payload" << std::endl;
    event_loop.run_forever();
    return 0;
}
