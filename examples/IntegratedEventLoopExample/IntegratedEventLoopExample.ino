#if defined(ARDUINO)
#define PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP
#define PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP
#include <Arduino.h>
#include <WiFi.h>
#ifndef PYPILOT_WIFI_SSID
#define PYPILOT_WIFI_SSID "ssid"
#endif
#ifndef PYPILOT_WIFI_PASSWORD
#define PYPILOT_WIFI_PASSWORD "password"
#endif
#ifndef PYPILOT_TCP_CLIENT_HOST
#define PYPILOT_TCP_CLIENT_HOST "192.168.1.10"
#endif
#ifndef PYPILOT_TCP_CLIENT_PORT
#define PYPILOT_TCP_CLIENT_PORT 20220
#endif
#ifndef PYPILOT_TCP_SERVER_PORT
#define PYPILOT_TCP_SERVER_PORT 20220
#endif
#ifndef PYPILOT_UDP_BROADCAST_HOST
#define PYPILOT_UDP_BROADCAST_HOST "255.255.255.255"
#endif
#ifndef PYPILOT_UDP_PORT
#define PYPILOT_UDP_PORT 20225
#endif
#ifndef PYPILOT_SERIAL_OUT
#define PYPILOT_SERIAL_OUT Serial1
#endif
#ifndef PYPILOT_PIN_READ
#define PYPILOT_PIN_READ 4
#endif
#ifndef PYPILOT_PIN_WRITE
#define PYPILOT_PIN_WRITE 5
#endif
#else
#include <cstdlib>
#include <iostream>
#endif

#include <string.h>
#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<48> event_loop;
NativeTcpServer tcp_server(event_loop.scheduler());
NativeTcpClient tcp_client(event_loop.scheduler());
NativeUdpDatagramStream udp;

#if defined(ARDUINO)
NativeSerialStream serial_in(Serial);
NativeSerialStream serial_out(PYPILOT_SERIAL_OUT);
#else
NativeSerialStream serial_in;
NativeSerialStream serial_out;
bool linux_pin_input = false;
bool linux_pin_output = false;
#endif

ITcpConnection* tcp_client_connection = nullptr;
uint32_t tcp_client_tx_count = 0;
uint32_t udp_tx_count = 0;
uint32_t serial_tx_count = 0;
bool pin_output_level = false;
bool last_pin_input_level = false;

static size_t text_len(const char* text) {
    size_t n = 0;
    while (text && text[n] != '\0') {
        ++n;
    }
    return n;
}

static void serial_write_text(const char* text) {
    serial_out.write(reinterpret_cast<const uint8_t*>(text), text_len(text));
}

static void serial_write_uint32(uint32_t value) {
    char digits[10];
    size_t n = 0;
    do {
        digits[n++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value > 0 && n < sizeof(digits));
    while (n > 0) {
        const char c = digits[--n];
        serial_out.write(reinterpret_cast<const uint8_t*>(&c), 1);
    }
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

static bool read_input_pin() {
#if defined(ARDUINO)
    return digitalRead(PYPILOT_PIN_READ) == HIGH;
#else
    linux_pin_input = !linux_pin_input;
    return linux_pin_input;
#endif
}

static void write_output_pin(bool high) {
#if defined(ARDUINO)
    digitalWrite(PYPILOT_PIN_WRITE, high ? HIGH : LOW);
#else
    linux_pin_output = high;
#endif
}

LineProtocolReader<128> serial_lines(serial_in, LineProtocolOptions{}, [](LineView line) {
    serial_write_text("serial rx: ");
    if (line.data && line.size > 0) {
        serial_out.write(reinterpret_cast<const uint8_t*>(line.data), line.size);
    }
    serial_write_text("\n");
});

struct ServerHandler final : public ITcpServerHandler {
    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)peer;
        tcp_write_line(connection, "server: accepted");
    }

    void on_data(ITcpConnection& connection) override {
        char line[160];
        while (connection.read_line(line, sizeof(line))) {
            serial_write_text("tcp server rx: ");
            serial_write_text(line);
            serial_write_text("\n");
            tcp_write_text(connection, "server rx: ");
            tcp_write_text(connection, line);
            tcp_write_newline(connection);
        }
    }
};

struct ClientHandler final : public ITcpClientHandler {
    void on_connect(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        (void)peer;
        tcp_client_connection = &connection;
        tcp_write_line(connection, "client: connected");
        serial_write_text("tcp client connected\n");
    }

    void on_data(ITcpConnection& connection) override {
        char line[160];
        while (connection.read_line(line, sizeof(line))) {
            serial_write_text("tcp client rx: ");
            serial_write_text(line);
            serial_write_text("\n");
            tcp_write_text(connection, "client rx: ");
            tcp_write_text(connection, line);
            tcp_write_newline(connection);
        }
    }

    void on_close(ITcpConnection& connection) override {
        (void)connection;
        tcp_client_connection = nullptr;
        serial_write_text("tcp client closed\n");
    }

    void on_error(int error_code) override {
        (void)error_code;
        tcp_client_connection = nullptr;
        serial_write_text("tcp client error\n");
    }
};

ServerHandler server_handler;
ClientHandler client_handler;

#if defined(ARDUINO)
static void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(PYPILOT_WIFI_SSID, PYPILOT_WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }
}
#endif

static bool setup_network(const char* client_host, uint16_t client_port, uint16_t server_port) {
    TcpListenOptions listen_options;
    listen_options.host = "0.0.0.0";
    listen_options.port = server_port;
    if (!tcp_server.listen(listen_options, server_handler)) {
        serial_write_text("tcp server listen failed\n");
        return false;
    }

    TcpConnectOptions connect_options;
    connect_options.host = client_host;
    connect_options.port = client_port;
    tcp_client.connect(connect_options, client_handler);

    if (!udp.bind(0)) {
        serial_write_text("udp bind failed\n");
        return false;
    }
    if (!udp.set_remote(PYPILOT_UDP_BROADCAST_HOST, PYPILOT_UDP_PORT)) {
        serial_write_text("udp remote failed\n");
        return false;
    }
    return true;
}

static void setup_tasks() {
    event_loop.on_bytes_ready(serial_in, []() {
        serial_lines.poll(event_loop.clock().micros());
    });

    event_loop.on_repeat(1000, []() {
        serial_write_text("serial tx: ");
        serial_write_uint32(serial_tx_count++);
        serial_write_text("\n");
    });

    event_loop.on_repeat(1000, []() {
        if (tcp_client_connection && tcp_client_connection->valid()) {
            tcp_write_text(*tcp_client_connection, "client tx: ");
            tcp_write_uint32(*tcp_client_connection, tcp_client_tx_count++);
            tcp_write_newline(*tcp_client_connection);
        }
    });

    event_loop.on_repeat(1000, []() {
        const char message[] = "integrated udp broadcast\n";
        udp.send(reinterpret_cast<const uint8_t*>(message), strlen(message));
        ++udp_tx_count;
    });

    event_loop.on_repeat(500, []() {
        const bool level = read_input_pin();
        if (level != last_pin_input_level) {
            last_pin_input_level = level;
            serial_write_text("pin read: ");
            serial_write_text(level ? "1\n" : "0\n");
        }
    });

    event_loop.on_repeat(1000, []() {
        pin_output_level = !pin_output_level;
        write_output_pin(pin_output_level);
        serial_write_text("pin write: ");
        serial_write_text(pin_output_level ? "1\n" : "0\n");
    });
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    PYPILOT_SERIAL_OUT.begin(115200);
    pinMode(PYPILOT_PIN_READ, INPUT_PULLUP);
    pinMode(PYPILOT_PIN_WRITE, OUTPUT);
    connect_wifi();
    setup_tasks();
    setup_network(PYPILOT_TCP_CLIENT_HOST, PYPILOT_TCP_CLIENT_PORT, PYPILOT_TCP_SERVER_PORT);
    serial_write_text("integrated event loop ready\n");
}

void loop() {
    event_loop.tick();
}
#else
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: integrated_event_loop_example SERIAL_RX_DEVICE SERIAL_TX_DEVICE [TCP_CLIENT_HOST] [TCP_CLIENT_PORT] [TCP_SERVER_PORT]" << std::endl;
        return 2;
    }
    const char* rx_device = argv[1];
    const char* tx_device = argv[2];
    const char* client_host = argc > 3 ? argv[3] : "127.0.0.1";
    const uint16_t client_port = argc > 4 ? static_cast<uint16_t>(std::strtoul(argv[4], nullptr, 10)) : 20220;
    const uint16_t server_port = argc > 5 ? static_cast<uint16_t>(std::strtoul(argv[5], nullptr, 10)) : 20220;

    if (!serial_in.open(rx_device, 115200)) {
        std::cerr << "failed to open serial input" << std::endl;
        return 1;
    }
    if (!serial_out.open(tx_device, 115200)) {
        std::cerr << "failed to open serial output" << std::endl;
        return 1;
    }

    setup_tasks();
    if (!setup_network(client_host, client_port, server_port)) {
        return 1;
    }
    serial_write_text("integrated event loop ready\n");
    event_loop.run_forever();
    return 0;
}
#endif
