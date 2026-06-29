#if defined(ARDUINO)
#define ASYNC_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP
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
#ifndef PYPILOT_UDP_BROADCAST_HOST
#define PYPILOT_UDP_BROADCAST_HOST "255.255.255.255"
#endif
#ifndef PYPILOT_UDP_PORT
#define PYPILOT_UDP_PORT 20225
#endif
#ifndef PYPILOT_UDP_LOCAL_PORT
#define PYPILOT_UDP_LOCAL_PORT 20226
#endif
#else
#include <cstdlib>
#include <iostream>
#endif

#include <string.h>
#include <async_event_loop.hpp>

using namespace async_event_loop;

EventLoop<> event_loop;
NativeUdpDatagramStream udp;
uint32_t sequence = 0;

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

static void print_number(uint32_t value) {
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

static bool setup_example(const char* host, uint16_t port, uint16_t local_port) {
#if defined(ARDUINO)
    if (!connect_wifi()) {
        return false;
    }
#endif
    if (!udp.bind(local_port)) {
        return false;
    }
    if (!udp.set_remote(host, port)) {
        return false;
    }
    event_loop.on_repeat(1000, []() {
        const char message[] = "pypilot udp broadcast\n";
        udp.write(reinterpret_cast<const uint8_t*>(message), strlen(message));
        ++sequence;
        print_text("sent udp datagram ");
        print_number(sequence);
        print_text("\n");
    });
    return true;
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    if (!setup_example(PYPILOT_UDP_BROADCAST_HOST, PYPILOT_UDP_PORT, PYPILOT_UDP_LOCAL_PORT)) {
        print_text("udp sender setup failed\n");
        setup_failed = true;
    }
}

void loop() {
    if (setup_failed) {
        print_text("udp sender setup failed\n");
        delay(1000);
        return;
    }
    event_loop.tick();
}
#else
int main(int argc, char** argv) {
    const char* host = argc > 1 ? argv[1] : "255.255.255.255";
    const uint16_t port = argc > 2 ? static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10)) : 20225;
    const uint16_t local_port = argc > 3 ? static_cast<uint16_t>(std::strtoul(argv[3], nullptr, 10)) : 0;
    if (!setup_example(host, port, local_port)) {
        std::cerr << "udp sender setup failed" << std::endl;
        return 1;
    }
    event_loop.run_forever();
    return 0;
}
#endif
