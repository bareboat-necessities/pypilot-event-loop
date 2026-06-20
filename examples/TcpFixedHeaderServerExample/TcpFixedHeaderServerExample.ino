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
#ifndef PYPILOT_WIFI_CONNECT_TIMEOUT_MS
#define PYPILOT_WIFI_CONNECT_TIMEOUT_MS 15000UL
#endif
#else
#include <iostream>
#endif

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

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

struct FixedHeaderServerHandler final : public ITcpServerHandler {
    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)connection;
        print_text("accepted ");
        print_text(peer.host);
        print_text(":");
        print_number(peer.port);
        print_text("\n");
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
};

static FixedHeaderServerHandler handler;

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
    options.port = 20221;
    options.reuse_address = true;

    if (!server.listen(options, handler)) {
        return false;
    }

    print_text("TCP fixed-header server listening on port ");
    print_number(server.port());
    print_text("\n");
    print_text("message format: one length byte followed by payload\n");
    return true;
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    if (!setup_example()) {
        print_text("tcp fixed-header server setup failed\n");
        setup_failed = true;
    }
}

void loop() {
    if (setup_failed) {
        print_text("tcp fixed-header server setup failed\n");
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
