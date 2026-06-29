#if defined(ARDUINO)
#define ASYNC_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP
#include <Arduino.h>
#include <WiFi.h>
#ifndef PYPILOT_WIFI_SSID
#define PYPILOT_WIFI_SSID "ssid"
#endif
#ifndef PYPILOT_WIFI_PASSWORD
#define PYPILOT_WIFI_PASSWORD "password"
#endif
#ifndef PYPILOT_WIFI_CONNECT_TIMEOUT_MS
#define PYPILOT_WIFI_CONNECT_TIMEOUT_MS 15000UL
#endif
#else
#include <iostream>
#endif

#include <string.h>
#include <async_event_loop.hpp>

using namespace async_event_loop;

static EventLoop<> event_loop;
static NativeTcpServer server(event_loop.scheduler());

#if defined(ARDUINO)
static bool setup_failed = false;
#endif

static void print_text(const char* text) {
#if defined(ARDUINO)
    Serial.print(text);
#else
    std::cout << text;
#endif
}

static void print_number(uint16_t value) {
#if defined(ARDUINO)
    Serial.print(value);
#else
    std::cout << value;
#endif
}

static void print_line_view(LineView line) {
#if defined(ARDUINO)
    Serial.write(reinterpret_cast<const uint8_t*>(line.data), line.size);
#else
    std::cout.write(line.data, static_cast<std::streamsize>(line.size));
#endif
}

class TcpConnectionByteStream final : public IByteStream {
public:
    void bind(ITcpConnection* connection) { connection_ = connection; }
    void unbind() { connection_ = nullptr; }

    int read(uint8_t* dst, size_t max_len) override {
        return connection_ ? connection_->read(dst, max_len) : 0;
    }

    int write(const uint8_t* src, size_t len) override {
        return connection_ ? connection_->write(src, len) : 0;
    }

    bool readable() const override {
        return connection_ && connection_->valid() && connection_->input_size() > 0;
    }

    bool writable() const override {
        return connection_ && connection_->valid();
    }

    bool valid() const override {
        return connection_ && connection_->valid();
    }

    ITcpConnection* connection() { return connection_; }

private:
    ITcpConnection* connection_ = nullptr;
};

struct ClientLineState {
    ClientLineState() : reader(stream, LineProtocolOptions{}, [this](LineView line) { on_line(line); }) {}

    bool in_use() const { return stream.connection() != nullptr; }

    void bind(ITcpConnection& connection) { stream.bind(&connection); }
    void unbind() { stream.unbind(); }

    bool owns(ITcpConnection& connection) const { return stream.connection() == &connection; }

    void poll(uint64_t now_us) { reader.poll(now_us); }

    void on_line(LineView line) {
        ITcpConnection* connection = stream.connection();
        if (!connection || !connection->valid()) {
            return;
        }

        print_text("line: ");
        print_line_view(line);
        print_text("\n");

        if (line.size == 8 && memcmp(line.data, "shutdown", 8) == 0) {
            server.close();
            event_loop.request_exit();
            return;
        }

        connection->write(reinterpret_cast<const uint8_t*>(line.data), line.size);
        const uint8_t newline = '\n';
        connection->write(&newline, 1);
    }

    TcpConnectionByteStream stream;
    LineProtocolReader<256> reader;
};

static ClientLineState clients[8];

static ClientLineState* find_client(ITcpConnection& connection) {
    for (size_t i = 0; i < sizeof(clients) / sizeof(clients[0]); ++i) {
        if (clients[i].owns(connection)) {
            return &clients[i];
        }
    }
    return nullptr;
}

static ClientLineState* acquire_client(ITcpConnection& connection) {
    ClientLineState* existing = find_client(connection);
    if (existing) {
        return existing;
    }
    for (size_t i = 0; i < sizeof(clients) / sizeof(clients[0]); ++i) {
        if (!clients[i].in_use()) {
            clients[i].bind(connection);
            return &clients[i];
        }
    }
    return nullptr;
}

struct LineServerHandler final : public ITcpServerHandler {
    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        ClientLineState* client = acquire_client(connection);
        if (!client) {
            print_text("too many clients\n");
            connection.close();
            return;
        }

        print_text("accepted ");
        print_text(peer.host);
        print_text(":");
        print_number(peer.port);
        print_text("\n");
    }

    void on_data(ITcpConnection& connection) override {
        ClientLineState* client = acquire_client(connection);
        if (!client) {
            print_text("too many clients\n");
            connection.close();
            return;
        }
        client->poll(event_loop.clock().micros());
    }

    void on_close(ITcpConnection& connection) override {
        print_text("closed ");
        print_text(connection.peer().host);
        print_text(":");
        print_number(connection.peer().port);
        print_text("\n");

        ClientLineState* client = find_client(connection);
        if (client) {
            client->unbind();
        }
    }

    void on_error(ITcpConnection& connection, int error_code) override {
        (void)error_code;
        print_text("connection error\n");

        ClientLineState* client = find_client(connection);
        if (client) {
            client->unbind();
        }
    }

    void on_listener_error(int error_code) override {
        (void)error_code;
        print_text("listener error\n");
    }
};

static LineServerHandler handler;

#if defined(ARDUINO)
static bool wifi_credentials_configured() {
    return strcmp(PYPILOT_WIFI_SSID, "ssid") != 0 && PYPILOT_WIFI_SSID[0] != '\0';
}

static bool connect_wifi() {
    if (!wifi_credentials_configured()) {
        print_text("wifi credentials are placeholders\n");
        return false;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(PYPILOT_WIFI_SSID, PYPILOT_WIFI_PASSWORD);
    const unsigned long start_ms = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start_ms >= PYPILOT_WIFI_CONNECT_TIMEOUT_MS) {
            print_text("wifi connect timeout\n");
            return false;
        }
        delay(250);
    }
    return true;
}
#endif

static bool setup_example() {
#if defined(ARDUINO)
    if (!connect_wifi()) {
        return false;
    }
#endif

    TcpListenOptions options;
    options.host = "0.0.0.0";
    options.port = 20220;
    options.reuse_address = true;

    if (!server.listen(options, handler)) {
        return false;
    }

    print_text("TCP line server listening on port ");
    print_number(server.port());
    print_text("\n");
    print_text("send 'shutdown' to stop\n");
    return true;
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    if (!setup_example()) {
        print_text("tcp server setup failed\n");
        setup_failed = true;
    }
}

void loop() {
    if (setup_failed) {
        print_text("tcp server setup failed\n");
        delay(1000);
        return;
    }
    event_loop.tick();
}
#else
int main() {
    if (!event_loop.valid()) {
        return 1;
    }
    if (!setup_example()) {
        return 2;
    }
    event_loop.run_forever();
    return 0;
}
#endif
