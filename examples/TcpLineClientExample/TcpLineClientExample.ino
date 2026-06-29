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
#ifndef PYPILOT_TCP_HOST
#define PYPILOT_TCP_HOST "192.168.1.10"
#endif
#ifndef PYPILOT_TCP_PORT
#define PYPILOT_TCP_PORT 20220
#endif
#else
#include <cstdlib>
#include <iostream>
#endif

#include <string.h>
#include <async_event_loop.hpp>

using namespace async_event_loop;

EventLoop<> event_loop;
NativeTcpClient client(event_loop.scheduler());
uint32_t tx_count = 0;

#if defined(ARDUINO)
bool setup_failed = false;
#endif

static void print_text(const char* text) {
#if defined(ARDUINO)
    Serial.print(text);
#else
    std::cout << text;
#endif
}

static void print_line_view(LineView line) {
#if defined(ARDUINO)
    Serial.write(reinterpret_cast<const uint8_t*>(line.data), line.size);
#else
    std::cout.write(line.data, static_cast<std::streamsize>(line.size));
#endif
}

static void tcp_write_text(ITcpConnection& connection, const char* text) {
    connection.write(reinterpret_cast<const uint8_t*>(text), strlen(text));
}

static void tcp_write_uint32(ITcpConnection& connection, uint32_t value) {
    char digits[10];
    size_t n = 0;
    do {
        digits[n++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value > 0 && n < sizeof(digits));
    while (n > 0) {
        const char c = digits[--n];
        connection.write(reinterpret_cast<const uint8_t*>(&c), 1);
    }
}

static void tcp_write_newline(ITcpConnection& connection) {
    const uint8_t newline = '\n';
    connection.write(&newline, 1);
}

static void tcp_write_line(ITcpConnection& connection, const char* text) {
    tcp_write_text(connection, text);
    tcp_write_newline(connection);
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
    const ITcpConnection* connection() const { return connection_; }

private:
    ITcpConnection* connection_ = nullptr;
};

static TcpConnectionByteStream inbound_stream;

static void handle_rx_line(LineView line) {
    ITcpConnection* connection = inbound_stream.connection();
    if (!connection || !connection->valid()) {
        return;
    }

    print_text("rx: ");
    print_line_view(line);
    print_text("\n");

    tcp_write_text(*connection, "rx: ");
    connection->write(reinterpret_cast<const uint8_t*>(line.data), line.size);
    tcp_write_newline(*connection);
}

static LineProtocolReader<160> inbound_lines(inbound_stream, LineProtocolOptions{}, [](LineView line) {
    handle_rx_line(line);
});

static void write_periodic_tcp_line() {
    ITcpConnection* connection = inbound_stream.connection();
    if (!connection || !connection->valid()) {
        return;
    }
    tcp_write_text(*connection, "tx: ");
    tcp_write_uint32(*connection, tx_count++);
    tcp_write_newline(*connection);
}

struct LineClientHandler final : public ITcpClientHandler {
    void on_connect(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)peer;
        inbound_stream.bind(&connection);
        print_text("connected\n");
        tcp_write_line(connection, "tx: connected");
    }

    void on_data(ITcpConnection& connection) override {
        (void)connection;
        inbound_lines.poll(event_loop.clock().micros());
    }

    void on_close(ITcpConnection& connection) override {
        (void)connection;
        inbound_stream.unbind();
        print_text("closed\n");
    }

    void on_error(int error_code) override {
        (void)error_code;
        inbound_stream.unbind();
        print_text("tcp client error\n");
    }
};

LineClientHandler handler;

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

void setup() {
    Serial.begin(115200);
    if (!connect_wifi()) {
        print_text("tcp client setup failed\n");
        setup_failed = true;
        return;
    }
    TcpConnectOptions options;
    options.host = PYPILOT_TCP_HOST;
    options.port = PYPILOT_TCP_PORT;
    if (!client.connect(options, handler)) {
        print_text("tcp client connect start failed\n");
        setup_failed = true;
        return;
    }
    event_loop.on_repeat(1000, []() {
        write_periodic_tcp_line();
    });
}

void loop() {
    if (setup_failed) {
        print_text("tcp client setup failed\n");
        delay(1000);
        return;
    }
    event_loop.tick();
}
#else
int main(int argc, char** argv) {
    TcpConnectOptions options;
    options.host = argc > 1 ? argv[1] : "127.0.0.1";
    options.port = argc > 2 ? static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10)) : 20220;
    if (!client.connect(options, handler)) {
        return 1;
    }
    event_loop.on_repeat(1000, []() {
        write_periodic_tcp_line();
    });
    event_loop.run_forever();
    return 0;
}
#endif
