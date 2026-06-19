#if defined(ARDUINO)
#define PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP
#include <Arduino.h>
#include <WiFi.h>
#ifndef PYPILOT_WIFI_SSID
#define PYPILOT_WIFI_SSID "ssid"
#endif
#ifndef PYPILOT_WIFI_PASSWORD
#define PYPILOT_WIFI_PASSWORD "password"
#endif
#else
#include <iostream>
#endif

#include <string.h>
#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

static EventLoop<> event_loop;
static NativeTcpServer server(event_loop.scheduler());

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

struct LineServerHandler final : public ITcpServerHandler {
    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)connection;
        print_text("accepted ");
        print_text(peer.host);
        print_text(":");
        print_number(peer.port);
        print_text("\n");
    }

    void on_data(ITcpConnection& connection) override {
        char line[256];
        while (connection.read_line(line, sizeof(line))) {
            print_text("line: ");
            print_text(line);
            print_text("\n");
            if (strcmp(line, "shutdown") == 0) {
                server.close();
                event_loop.request_exit();
                return;
            }
            connection.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
            const uint8_t newline = '\n';
            connection.write(&newline, 1);
        }
    }

    void on_close(ITcpConnection& connection) override {
        print_text("closed ");
        print_text(connection.peer().host);
        print_text(":");
        print_number(connection.peer().port);
        print_text("\n");
    }

    void on_error(ITcpConnection& connection, int error_code) override {
        (void)connection;
        (void)error_code;
        print_text("connection error\n");
    }

    void on_listener_error(int error_code) override {
        (void)error_code;
        print_text("listener error\n");
    }
};

static LineServerHandler handler;

#if defined(ARDUINO)
static void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(PYPILOT_WIFI_SSID, PYPILOT_WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }
}
#endif

static bool setup_example() {
#if defined(ARDUINO)
    connect_wifi();
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
    setup_example();
}

void loop() {
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
