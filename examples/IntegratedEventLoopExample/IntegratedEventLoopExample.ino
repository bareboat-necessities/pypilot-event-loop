#if defined(ARDUINO)
#define ASYNC_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP
#define ASYNC_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP
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
#ifndef PYPILOT_TCP_CLIENT_HOST
#define PYPILOT_TCP_CLIENT_HOST "192.168.1.10"
#endif
#ifndef PYPILOT_TCP_CLIENT_PORT
#define PYPILOT_TCP_CLIENT_PORT 20220
#endif
#ifndef PYPILOT_TCP_SERVER_PORT
#define PYPILOT_TCP_SERVER_PORT 20220
#endif
#ifndef PYPILOT_SERIAL_OUT
#define PYPILOT_SERIAL_OUT Serial1
#endif
#ifndef PYPILOT_SERIAL_OUT_RX_PIN
#define PYPILOT_SERIAL_OUT_RX_PIN 18
#endif
#ifndef PYPILOT_SERIAL_OUT_TX_PIN
#define PYPILOT_SERIAL_OUT_TX_PIN 17
#endif
#ifndef PYPILOT_DIGITAL_IN_PIN
#define PYPILOT_DIGITAL_IN_PIN 4
#endif
#ifndef PYPILOT_DIGITAL_OUT_PIN
#define PYPILOT_DIGITAL_OUT_PIN 5
#endif
#ifndef PYPILOT_ANALOG_IN_PIN
#define PYPILOT_ANALOG_IN_PIN 1
#endif
#ifndef PYPILOT_ANALOG_OUT_PIN
#define PYPILOT_ANALOG_OUT_PIN 2
#endif
#else
#include <cstdlib>
#include <iostream>
#endif

#ifndef PYPILOT_UDP_BROADCAST_HOST
#define PYPILOT_UDP_BROADCAST_HOST "255.255.255.255"
#endif
#ifndef PYPILOT_UDP_PORT
#define PYPILOT_UDP_PORT 20225
#endif

#include <string.h>
#include <async_event_loop.hpp>

namespace ael = async_event_loop;

ael::EventLoop<48> event_loop;
ael::NativeTcpServer tcp_server(event_loop.scheduler());
ael::NativeTcpClient tcp_client(event_loop.scheduler());
ael::NativeUdpDatagramStream udp;

#if defined(ARDUINO)
ael::NativeSerialStream serial_in(Serial);
ael::NativeSerialStream serial_out(PYPILOT_SERIAL_OUT);
#endif

#if !defined(ARDUINO)
ael::NativeSerialStream serial_in;
ael::NativeSerialStream serial_out;
#endif

ael::IDigitalInputPin* digital_in = nullptr;
ael::IDigitalOutputPin* digital_out = nullptr;
ael::IAnalogInputPin* analog_in = nullptr;
ael::IAnalogOutputPin* analog_out = nullptr;

ael::ITcpConnection* tcp_client_connection = nullptr;
uint32_t tcp_client_tx_count = 0;
uint32_t udp_tx_count = 0;
uint32_t serial_tx_count = 0;
bool digital_output_level = false;
bool last_digital_input_level = false;
int analog_output_value = 0;
int analog_output_step = 32;

#if defined(ARDUINO)
bool setup_failed = false;
#endif

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

static void serial_write_line(ael::LineView line) {
    if (line.data && line.size > 0) {
        serial_out.write(reinterpret_cast<const uint8_t*>(line.data), line.size);
    }
    serial_write_text("\n");
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

static void serial_write_int(int value) {
    if (value < 0) {
        const char minus = '-';
        serial_out.write(reinterpret_cast<const uint8_t*>(&minus), 1);
        value = -value;
    }
    serial_write_uint32(static_cast<uint32_t>(value));
}

static void tcp_write_uint32(ael::ITcpConnection& connection, uint32_t value) {
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

#if defined(ARDUINO)
static bool setup_pins() {
    static ael::NativeDigitalInputPin digital_in_pin(PYPILOT_DIGITAL_IN_PIN, ael::DigitalPinMode::InputPullup);
    static ael::NativeDigitalOutputPin digital_out_pin(PYPILOT_DIGITAL_OUT_PIN, false);
    static ael::NativeAnalogInputPin analog_in_pin(PYPILOT_ANALOG_IN_PIN);
    static ael::NativeAnalogOutputPin analog_out_pin(PYPILOT_ANALOG_OUT_PIN);
    digital_in = &digital_in_pin;
    digital_out = &digital_out_pin;
    analog_in = &analog_in_pin;
    analog_out = &analog_out_pin;
    return digital_in->valid() && digital_out->valid() && analog_in->valid() && analog_out->valid();
}
#else
static bool setup_pins(const char* digital_in_path,
                       const char* digital_out_path,
                       const char* analog_in_path,
                       const char* analog_out_path) {
    static ael::NativeDigitalInputPin digital_in_pin(digital_in_path);
    static ael::NativeDigitalOutputPin digital_out_pin(digital_out_path);
    static ael::NativeAnalogInputPin analog_in_pin(analog_in_path);
    static ael::NativeAnalogOutputPin analog_out_pin(analog_out_path);
    digital_in = &digital_in_pin;
    digital_out = &digital_out_pin;
    analog_in = &analog_in_pin;
    analog_out = &analog_out_pin;
    return digital_in->valid() && digital_out->valid() && analog_in->valid() && analog_out->valid();
}
#endif

static bool read_digital_input_pin() {
    return digital_in && digital_in->valid() && digital_in->read();
}

static void write_digital_output_pin(bool high) {
    if (digital_out && digital_out->valid()) {
        digital_out->write(high);
    }
}

static int read_analog_input_pin() {
    if (!analog_in || !analog_in->valid()) {
        return 0;
    }
    return analog_in->read();
}

static void write_analog_output_pin(int value) {
    if (analog_out && analog_out->valid()) {
        analog_out->write(value);
    }
}

ael::LineProtocolReader<128> serial_lines(serial_in, ael::LineProtocolOptions{}, [](ael::LineView line) {
    serial_write_text("serial rx: ");
    serial_write_line(line);
});

struct ServerLineHandler final : public ael::ITcpLineServerHandler {
    void on_accept(ael::ITcpConnection& connection, const ael::TcpPeerInfo& peer) override {
        (void)peer;
        ael::tcp_write_line(connection, "server: accepted");
    }

    void on_line(ael::ITcpConnection& connection, ael::LineView line) override {
        serial_write_text("tcp server rx: ");
        serial_write_line(line);
        ael::tcp_write_text(connection, "server rx: ");
        ael::tcp_write_line(connection, line);
    }

    void on_close(ael::ITcpConnection& connection) override {
        (void)connection;
        serial_write_text("tcp server connection closed\n");
    }

    void on_error(ael::ITcpConnection& connection, int error_code) override {
        (void)connection;
        (void)error_code;
        serial_write_text("tcp server connection error\n");
    }

    void on_listener_error(int error_code) override {
        (void)error_code;
        serial_write_text("tcp server listener error\n");
    }
};

struct ClientLineHandler final : public ael::ITcpLineClientHandler {
    void on_connect(ael::ITcpConnection& connection, const ael::TcpPeerInfo& peer) override {
        (void)peer;
        tcp_client_connection = &connection;
        ael::tcp_write_line(connection, "client: connected");
        serial_write_text("tcp client connected\n");
    }

    void on_line(ael::ITcpConnection& connection, ael::LineView line) override {
        (void)connection;
        serial_write_text("tcp client rx: ");
        serial_write_line(line);
    }

    void on_close(ael::ITcpConnection& connection) override {
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

ServerLineHandler server_line_handler;
ael::TcpLineServerHandler<160> server_handler(server_line_handler);
ClientLineHandler client_line_handler;
ael::TcpLineClientHandler<160> client_handler(client_line_handler);

static bool setup_network(const char* client_host, uint16_t client_port, uint16_t server_port) {
    ael::TcpListenOptions listen_options;
    listen_options.host = "0.0.0.0";
    listen_options.port = server_port;
    if (!tcp_server.listen(listen_options, server_handler)) {
        serial_write_text("tcp server listen failed\n");
        return false;
    }

    ael::TcpConnectOptions connect_options;
    connect_options.host = client_host;
    connect_options.port = client_port;
    if (!tcp_client.connect(connect_options, client_handler)) {
        serial_write_text("tcp client connect start failed\n");
        return false;
    }

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
            ael::tcp_write_text(*tcp_client_connection, "client tx: ");
            tcp_write_uint32(*tcp_client_connection, tcp_client_tx_count++);
            ael::tcp_write_newline(*tcp_client_connection);
        }
    });

    event_loop.on_repeat(1000, []() {
        const char message[] = "integrated udp broadcast\n";
        udp.write(reinterpret_cast<const uint8_t*>(message), strlen(message));
        ++udp_tx_count;
    });

    event_loop.on_repeat(500, []() {
        const bool level = read_digital_input_pin();
        if (level != last_digital_input_level) {
            last_digital_input_level = level;
            serial_write_text("digital read: ");
            serial_write_text(level ? "1\n" : "0\n");
        }
    });

    event_loop.on_repeat(1000, []() {
        digital_output_level = !digital_output_level;
        write_digital_output_pin(digital_output_level);
        serial_write_text("digital write: ");
        serial_write_text(digital_output_level ? "1\n" : "0\n");
    });

    event_loop.on_repeat(1000, []() {
        serial_write_text("analog read: ");
        serial_write_int(read_analog_input_pin());
        serial_write_text("\n");
    });

    event_loop.on_repeat(1000, []() {
        write_analog_output_pin(analog_output_value);
        serial_write_text("analog write: ");
        serial_write_int(analog_output_value);
        serial_write_text("\n");
        analog_output_value += analog_output_step;
        const int max_value = analog_out ? analog_out->max_value() : 255;
        if (analog_output_value >= max_value) {
            analog_output_value = max_value;
            analog_output_step = -analog_output_step;
        } else if (analog_output_value <= 0) {
            analog_output_value = 0;
            analog_output_step = -analog_output_step;
        }
    });
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
#if defined(ESP32)
    PYPILOT_SERIAL_OUT.begin(115200, SERIAL_8N1, PYPILOT_SERIAL_OUT_RX_PIN, PYPILOT_SERIAL_OUT_TX_PIN);
#else
    PYPILOT_SERIAL_OUT.begin(115200);
#endif
    const ael::WiFiConnectResult wifi_result = ael::connect_wifi_result(
        PYPILOT_WIFI_SSID,
        PYPILOT_WIFI_PASSWORD,
        PYPILOT_WIFI_CONNECT_TIMEOUT_MS
    );
    if (wifi_result != ael::WiFiConnectResult::Connected) {
        serial_write_text(ael::wifi_connect_result_text(wifi_result));
        serial_write_text("\n");
        serial_write_text("integrated event loop wifi setup failed\n");
        setup_failed = true;
        return;
    }
    if (!setup_pins()) {
        serial_write_text("integrated event loop pin setup failed\n");
        setup_failed = true;
        return;
    }
    setup_tasks();
    if (!setup_network(PYPILOT_TCP_CLIENT_HOST, PYPILOT_TCP_CLIENT_PORT, PYPILOT_TCP_SERVER_PORT)) {
        serial_write_text("integrated event loop network setup failed\n");
        setup_failed = true;
        return;
    }
    serial_write_text("integrated event loop ready\n");
}

void loop() {
    if (setup_failed) {
        serial_write_text("integrated event loop setup failed\n");
        delay(1000);
        return;
    }
    event_loop.tick();
}
#else
int main(int argc, char** argv) {
    if (argc < 7) {
        std::cerr << "usage: integrated_event_loop_example SERIAL_RX_DEVICE SERIAL_TX_DEVICE DIGITAL_IN_FILE DIGITAL_OUT_FILE ANALOG_IN_FILE ANALOG_OUT_FILE [TCP_CLIENT_HOST] [TCP_CLIENT_PORT] [TCP_SERVER_PORT]" << std::endl;
        return 2;
    }
    const char* rx_device = argv[1];
    const char* tx_device = argv[2];
    const char* digital_in_path = argv[3];
    const char* digital_out_path = argv[4];
    const char* analog_in_path = argv[5];
    const char* analog_out_path = argv[6];
    const char* client_host = argc > 7 ? argv[7] : "localhost";
    const uint16_t client_port = argc > 8 ? static_cast<uint16_t>(std::strtoul(argv[8], nullptr, 10)) : 20220;
    const uint16_t server_port = argc > 9 ? static_cast<uint16_t>(std::strtoul(argv[9], nullptr, 10)) : 20220;

    if (!serial_in.open(rx_device, 115200)) {
        std::cerr << "failed to open serial input" << std::endl;
        return 1;
    }
    if (!serial_out.open(tx_device, 115200)) {
        std::cerr << "failed to open serial output" << std::endl;
        return 1;
    }
    if (!setup_pins(digital_in_path, digital_out_path, analog_in_path, analog_out_path)) {
        std::cerr << "failed to set up pin files" << std::endl;
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
