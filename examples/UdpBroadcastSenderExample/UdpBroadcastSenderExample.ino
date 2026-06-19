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
#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;
NativeUdpDatagramStream udp;
uint32_t sequence = 0;

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
static void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(PYPILOT_WIFI_SSID, PYPILOT_WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }
}
#endif

static bool setup_example(const char* broadcast_host, uint16_t port, uint16_t local_port) {
#if defined(ARDUINO)
    connect_wifi();
#endif
    if (!udp.bind(local_port)) {
        return false;
    }
    if (!udp.set_remote(broadcast_host, port)) {
        return false;
    }
    event_loop.on_repeat(1000, []() {
        char message[96];
#if defined(ARDUINO)
        snprintf(message, sizeof(message), "pypilot udp broadcast %lu\n", static_cast<unsigned long>(sequence++));
#else
        snprintf(message, sizeof(message), "pypilot udp broadcast %u\n", sequence++);
#endif
        udp.send(reinterpret_cast<const uint8_t*>(message), strlen(message));
        print_text("sent broadcast ");
        print_number(sequence);
        print_text("\n");
    });
    return true;
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    if (!setup_example(PYPILOT_UDP_BROADCAST_HOST, PYPILOT_UDP_PORT, PYPILOT_UDP_LOCAL_PORT)) {
        print_text("udp broadcast sender setup failed\n");
    }
}

void loop() {
    event_loop.tick();
}
#else
int main(int argc, char** argv) {
    const char* host = argc > 1 ? argv[1] : "255.255.255.255";
    const uint16_t port = argc > 2 ? static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10)) : 20225;
    const uint16_t local_port = argc > 3 ? static_cast<uint16_t>(std::strtoul(argv[3], nullptr, 10)) : 0;
    if (!setup_example(host, port, local_port)) {
        std::cerr << "udp broadcast sender setup failed" << std::endl;
        return 1;
    }
    event_loop.run_forever();
    return 0;
}
#endif
