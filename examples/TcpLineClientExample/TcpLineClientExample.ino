#if defined(ARDUINO)
#define ASYNC_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP
#include <Arduino.h>
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

struct LineCallbacks final : public ITcpLineClientHandler {
    void on_connect(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)peer;
        print_text("connected\n");
        tcp_write_line(connection, "tx: connected");
    }

    void on_line(ITcpConnection& connection, LineView line) override {
        char text[161];
        print_text("rx: ");
        print_text(line_to_cstr(line, text));
        print_text("\n");

        tcp_write_text(connection, "rx: ");
        connection.write(reinterpret_cast<const uint8_t*>(line.data), line.size);
        tcp_write_newline(connection);
    }

    void on_close(ITcpConnection& connection) override {
        (void)connection;
        print_text("closed\n");
    }

    void on_error(int error_code) override {
        (void)error_code;
        print_text("tcp client error\n");
    }
};

static LineCallbacks line_callbacks;
static TcpLineClientHandler<160> handler(line_callbacks);

static void write_periodic_tcp_line() {
    ITcpConnection* connection = handler.connection();
    if (!connection || !connection->valid()) {
        return;
    }
    tcp_write_text(*connection, "tx: ");
    tcp_write_uint32(*connection, tx_count++);
    tcp_write_newline(*connection);
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    const WiFiConnectResult wifi_result = connect_wifi_result(
        PYPILOT_WIFI_SSID,
        PYPILOT_WIFI_PASSWORD,
        PYPILOT_WIFI_CONNECT_TIMEOUT_MS
    );
    if (wifi_result != WiFiConnectResult::Connected) {
        print_text(wifi_connect_result_text(wifi_result));
        print_text("\n");
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
