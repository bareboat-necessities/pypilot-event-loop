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

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;
NativeTcpClient client(event_loop.scheduler());

static void print_text(const char* text) {
#if defined(ARDUINO)
    Serial.print(text);
#else
    std::cout << text;
#endif
}

static void write_line(ITcpConnection& connection, const char* text) {
    connection.write(reinterpret_cast<const uint8_t*>(text), strlen(text));
    const uint8_t newline = '\n';
    connection.write(&newline, 1);
}

struct LineClientHandler final : public ITcpClientHandler {
    void on_connect(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)peer;
        print_text("connected\n");
        write_line(connection, "hello from pypilot-event-loop");
    }

    void on_data(ITcpConnection& connection) override {
        char line[160];
        while (connection.read_line(line, sizeof(line))) {
            print_text("line: ");
            print_text(line);
            print_text("\n");
        }
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

LineClientHandler handler;

#if defined(ARDUINO)
static void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(PYPILOT_WIFI_SSID, PYPILOT_WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }
}

void setup() {
    Serial.begin(115200);
    connect_wifi();
    TcpConnectOptions options;
    options.host = PYPILOT_TCP_HOST;
    options.port = PYPILOT_TCP_PORT;
    client.connect(options, handler);
}

void loop() {
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
    event_loop.run_forever();
    return 0;
}
#endif
