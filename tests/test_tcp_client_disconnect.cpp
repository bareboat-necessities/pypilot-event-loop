#include <cassert>

#include "async_event_loop.hpp"

struct ClosingServerHandler final : public async_event_loop::ITcpServerHandler {
    int accepted = 0;

    void on_accept(async_event_loop::ITcpConnection& connection,
                   const async_event_loop::TcpPeerInfo& peer) override {
        (void)peer;
        ++accepted;
        connection.close();
    }
};

struct ClientHandler final : public async_event_loop::ITcpClientHandler {
    int connected = 0;
    int closed = 0;
    int errors = 0;

    void on_connect(async_event_loop::ITcpConnection& connection,
                    const async_event_loop::TcpPeerInfo& peer) override {
        (void)connection;
        (void)peer;
        ++connected;
    }

    void on_close(async_event_loop::ITcpConnection& connection) override {
        (void)connection;
        ++closed;
    }

    void on_error(int error_code) override {
        (void)error_code;
        ++errors;
    }
};

int main() {
    async_event_loop::EventLoop<> event_loop;
    ClosingServerHandler server_handler;
    ClientHandler client_handler;

    async_event_loop::NativeTcpServer server(event_loop.scheduler());
    async_event_loop::TcpListenOptions listen_options;
    listen_options.host = "127.0.0.1";
    listen_options.port = 0;
    assert(server.listen(listen_options, server_handler));

    async_event_loop::NativeTcpClient client(event_loop.scheduler());
    async_event_loop::TcpConnectOptions connect_options;
    connect_options.host = "localhost";
    connect_options.port = server.port();
    assert(client.connect(connect_options, client_handler));

    for (int i = 0; i < 200 && client_handler.connected == 0 && client_handler.errors == 0; ++i) {
        event_loop.run_once();
    }
    assert(client_handler.connected == 1);
    assert(client_handler.errors == 0);

    for (int i = 0; i < 200 && client_handler.closed == 0 && client_handler.errors == 0; ++i) {
        event_loop.run_once();
    }

    assert(server_handler.accepted == 1);
    assert(client_handler.closed == 1);
    assert(client_handler.errors == 0);
    assert(!client.connected());
    assert(server.connection_count() == 0);
    return 0;
}
