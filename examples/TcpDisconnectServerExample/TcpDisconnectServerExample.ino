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
#else
#include <iostream>
#endif

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

static void print_size(size_t value) {
#if defined(ARDUINO)
    Serial.print(static_cast<unsigned long>(value));
#else
    std::cout << value;
#endif
}

struct DisconnectHandler final : public ITcpServerHandler {
    TcpConnectionRegistry<8> connections;

    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        if (!connections.add(connection)) {
            print_text("too many clients\n");
            connection.close();
            return;
        }
        print_text("accepted ");
        print_text(peer.host);
        print_text(":");
        print_number(peer.port);
        print_text(" active=");
        print_size(connections.size());
        print_text("\n");
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
            print_text("received bytes\n");
        }
    }

    void on_close(ITcpConnection& connection) override {
        connections.remove(connection);
        print_text("peer disconnected ");
        print_text(connection.peer().host);
        print_text(":");
        print_number(connection.peer().port);
        print_text(" active=");
        print_size(connections.size());
        print_text("\n");
    }

    void on_error(ITcpConnection& connection, int error_code) override {
        (void)error_code;
        connections.remove(connection);
        print_text("connection error active=");
        print_size(connections.size());
        print_text("\n");
    }

    void on_listener_error(int error_code) override {
        (void)error_code;
        print_text("listener error\n");
    }
};

static DisconnectHandler handler;

static bool setup_example() {
#if defined(ARDUINO)
    const WiFiConnectResult wifi_result = connect_wifi_result(
        PYPILOT_WIFI_SSID,
        PYPILOT_WIFI_PASSWORD,
        PYPILOT_WIFI_CONNECT_TIMEOUT_MS
    );
    if (wifi_result != WiFiConnectResult::Connected) {
        print_text(wifi_connect_result_text(wifi_result));
        print_text("\n");
        return false;
    }
#endif

    TcpListenOptions options;
    options.host = "0.0.0.0";
    options.port = 20222;
    options.reuse_address = true;

    if (!server.listen(options, handler)) {
        return false;
    }

    print_text("TCP disconnect detection server listening on port ");
    print_number(server.port());
    print_text("\n");
    return true;
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    if (!setup_example()) {
        print_text("tcp disconnect server setup failed\n");
        setup_failed = true;
    }
}

void loop() {
    if (setup_failed) {
        print_text("tcp disconnect server setup failed\n");
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
