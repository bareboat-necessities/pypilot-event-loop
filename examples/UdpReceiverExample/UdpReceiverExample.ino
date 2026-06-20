#if defined(ARDUINO)
#define PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP
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
#ifndef PYPILOT_UDP_PORT
#define PYPILOT_UDP_PORT 20225
#endif
#else
#include <cstdlib>
#include <iostream>
#endif

#include <string.h>
#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;
NativeUdpDatagramStream udp;

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

static void print_number(uint16_t value) {
#if defined(ARDUINO)
    Serial.print(value);
#else
    std::cout << value;
#endif
}

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

static bool setup_example(uint16_t port) {
#if defined(ARDUINO)
    if (!connect_wifi()) {
        return false;
    }
#endif
    if (!udp.bind(port)) {
        return false;
    }
    event_loop.on_bytes_ready(udp, []() {
        uint8_t buffer[256];
        UdpEndpoint source;
        while (udp.readable()) {
            const int n = udp.recv_from(buffer, sizeof(buffer) - 1, &source);
            if (n <= 0) {
                break;
            }
            buffer[n] = 0;
            print_text("from ");
            print_text(source.host);
            print_text(":");
            print_number(source.port);
            print_text(" ");
            print_text(reinterpret_cast<const char*>(buffer));
        }
    });
    return true;
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    if (!setup_example(PYPILOT_UDP_PORT)) {
        print_text("udp receiver setup failed\n");
        setup_failed = true;
    }
}

void loop() {
    if (setup_failed) {
        print_text("udp receiver setup failed\n");
        delay(1000);
        return;
    }
    event_loop.tick();
}
#else
int main(int argc, char** argv) {
    const uint16_t port = argc > 1 ? static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10)) : 20225;
    if (!setup_example(port)) {
        std::cerr << "udp receiver setup failed" << std::endl;
        return 1;
    }
    event_loop.run_forever();
    return 0;
}
#endif
