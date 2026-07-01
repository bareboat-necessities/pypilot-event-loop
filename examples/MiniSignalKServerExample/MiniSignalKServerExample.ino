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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <async_event_loop.hpp>

using namespace async_event_loop;

static constexpr uint16_t signalk_port = 20223;
static constexpr uint16_t nmea_port = 20224;

static EventLoop<> event_loop;
static NativeTcpServer signalk_server(event_loop.scheduler());
static NativeTcpServer nmea_server(event_loop.scheduler());

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

static void print_float(float value) {
    char text[32];
    snprintf(text, sizeof(text), "%.3f", static_cast<double>(value));
    print_text(text);
}

struct WindState {
    float angle_apparent_rad = 0.0f;
    float speed_apparent_m_s = 0.0f;
    uint64_t last_update_us = 0;
    bool valid = false;
};

class NmeaTokenizer {
public:
    static constexpr int max_fields = 8;

    bool tokenize(char* text) {
        reset();
        if (!text) {
            return false;
        }

        char* field_start = text;
        while (field_count_ < max_fields) {
            fields_[field_count_++] = field_start;
            char* comma = strchr(field_start, ',');
            if (!comma) {
                break;
            }
            *comma = '\0';
            field_start = comma + 1;
        }
        return field_count_ > 0;
    }

    int field_count() const { return field_count_; }

    const char* field(int index) const {
        if (index < 0 || index >= field_count_ || !fields_[index]) {
            return "";
        }
        return fields_[index];
    }

private:
    void reset() {
        for (int i = 0; i < max_fields; ++i) {
            fields_[i] = nullptr;
        }
        field_count_ = 0;
    }

    char* fields_[max_fields]{};
    int field_count_ = 0;
};

static bool parse_mwv(const NmeaTokenizer& tokenizer, WindState& wind, uint64_t now_us) {
    if (tokenizer.field_count() < 6 || strlen(tokenizer.field(0)) < 3) {
        return false;
    }

    const char* talker_sentence = tokenizer.field(0);
    const size_t sentence_len = strlen(talker_sentence);
    const char* sentence = sentence_len >= 3 ? talker_sentence + sentence_len - 3 : talker_sentence;
    if (strcmp(sentence, "MWV") != 0) {
        return false;
    }
    if (strcmp(tokenizer.field(2), "R") != 0 || tokenizer.field(5)[0] != 'A') {
        return false;
    }

    const char* angle_field = tokenizer.field(1);
    const char* speed_field = tokenizer.field(3);
    char* end_angle = nullptr;
    char* end_speed = nullptr;
    const float angle_deg = static_cast<float>(strtod(angle_field, &end_angle));
    const float speed_value = static_cast<float>(strtod(speed_field, &end_speed));
    if (end_angle == angle_field || end_speed == speed_field) {
        return false;
    }

    float speed_factor = 1.0f;
    if (strcmp(tokenizer.field(4), "N") == 0) {
        speed_factor = 0.514444f;
    } else if (strcmp(tokenizer.field(4), "M") == 0) {
        speed_factor = 1.0f;
    } else if (strcmp(tokenizer.field(4), "K") == 0) {
        speed_factor = 1000.0f / 3600.0f;
    } else {
        return false;
    }

    wind.angle_apparent_rad = angle_deg * 3.14159265358979323846f / 180.0f;
    wind.speed_apparent_m_s = speed_value * speed_factor;
    wind.last_update_us = now_us;
    wind.valid = true;
    return true;
}

static TcpLineServerOptions make_signalk_options() {
    TcpLineServerOptions options;
#if defined(ARDUINO)
    options.backpressure = TcpBackpressureOptions::embedded_default();
#else
    options.backpressure = TcpBackpressureOptions::server_default();
#endif
    return options;
}

struct SignalKCallbacks final : public ITcpLineServerHandler {
    TcpConnectionRegistry<8> clients;
    int accepted = 0;
    int incoming_lines = 0;
    int broadcasts = 0;
    int backpressure_disconnects = 0;

    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        if (!clients.add(connection)) {
            print_text("too many Signal K clients\n");
            connection.close();
            return;
        }
        ++accepted;
        print_text("Signal K client accepted ");
        print_text(peer.host);
        print_text(":");
        print_number(peer.port);
        print_text(" active=");
        print_size(clients.size());
        print_text("\n");
    }

    void on_line(ITcpConnection& connection, LineView line) override {
        (void)connection;
        (void)line;
        ++incoming_lines;
        print_text("Signal K input line ignored\n");
    }

    void on_backpressure(ITcpConnection& connection, const TcpBackpressureInfo& info) override {
        ++backpressure_disconnects;
        print_text("Signal K backpressure close pending=");
        print_size(info.pending_bytes);
        print_text("\n");
        clients.remove(connection);
    }

    void on_close(ITcpConnection& connection) override {
        clients.remove(connection);
        print_text("Signal K client closed active=");
        print_size(clients.size());
        print_text("\n");
    }

    void on_error(ITcpConnection& connection, int error_code) override {
        clients.remove(connection);
        print_text("Signal K client error code=");
        print_number(static_cast<uint16_t>(error_code < 0 ? -error_code : error_code));
        print_text(" active=");
        print_size(clients.size());
        print_text("\n");
    }

    void on_listener_error(int error_code) override {
        print_text("Signal K listener error code=");
        print_number(static_cast<uint16_t>(error_code < 0 ? -error_code : error_code));
        print_text("\n");
    }

    void on_too_many_connections(ITcpConnection& connection) override {
        print_text("Signal K line handler full\n");
        connection.close();
    }

    void broadcast_wind_update(const WindState& wind) {
        if (!wind.valid) {
            return;
        }

        char json[512];
        const int len = snprintf(
            json,
            sizeof(json),
            "{\"updates\":[{\"source\":{\"label\":\"mini-signalk\"},"
            "\"values\":[{\"path\":\"environment.wind.angleApparent\",\"value\":%.6f},"
            "{\"path\":\"environment.wind.speedApparent\",\"value\":%.6f}]}]}\n",
            static_cast<double>(wind.angle_apparent_rad),
            static_cast<double>(wind.speed_apparent_m_s));
        if (len <= 0 || len >= static_cast<int>(sizeof(json))) {
            return;
        }

        clients.for_each([&](ITcpConnection& peer) {
            peer.write(reinterpret_cast<const uint8_t*>(json), static_cast<size_t>(len));
        });
        ++broadcasts;
    }
};

struct NmeaCallbacks final : public ITcpLineServerHandler {
    explicit NmeaCallbacks(SignalKCallbacks& sink) : signalk(sink) {}

    SignalKCallbacks& signalk;
    TcpConnectionRegistry<4> sources;
    WindState wind;
    int accepted = 0;
    int updates = 0;

    void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) override {
        if (!sources.add(connection)) {
            print_text("too many NMEA sources\n");
            connection.close();
            return;
        }
        ++accepted;
        print_text("NMEA source accepted ");
        print_text(peer.host);
        print_text(":");
        print_number(peer.port);
        print_text(" active=");
        print_size(sources.size());
        print_text("\n");
    }

    void on_line(ITcpConnection& connection, LineView line) override {
        (void)connection;

        char text[160];
        line_to_cstr(line, text);

        NmeaTokenizer tokenizer;
        if (!tokenizer.tokenize(text)) {
            return;
        }

        if (!parse_mwv(tokenizer, wind, event_loop.clock().micros())) {
            return;
        }

        ++updates;
        print_text("NMEA MWV update angle_rad=");
        print_float(wind.angle_apparent_rad);
        print_text(" speed_m_s=");
        print_float(wind.speed_apparent_m_s);
        print_text(" SignalK clients=");
        print_size(signalk.clients.size());
        print_text("\n");

        signalk.broadcast_wind_update(wind);
    }

    void on_close(ITcpConnection& connection) override {
        sources.remove(connection);
        print_text("NMEA source closed active=");
        print_size(sources.size());
        print_text("\n");
    }

    void on_error(ITcpConnection& connection, int error_code) override {
        sources.remove(connection);
        print_text("NMEA source error code=");
        print_number(static_cast<uint16_t>(error_code < 0 ? -error_code : error_code));
        print_text(" active=");
        print_size(sources.size());
        print_text("\n");
    }

    void on_listener_error(int error_code) override {
        print_text("NMEA listener error code=");
        print_number(static_cast<uint16_t>(error_code < 0 ? -error_code : error_code));
        print_text("\n");
    }

    void on_too_many_connections(ITcpConnection& connection) override {
        print_text("NMEA line handler full\n");
        connection.close();
    }
};

static SignalKCallbacks signalk_callbacks;
static NmeaCallbacks nmea_callbacks(signalk_callbacks);
static TcpLineServerOptions signalk_options = make_signalk_options();
static TcpLineServerOptions nmea_options;
static TcpLineServerHandler<256, 8> signalk_handler(signalk_callbacks, signalk_options);
static TcpLineServerHandler<192, 4> nmea_handler(nmea_callbacks, nmea_options);

static bool listen_server(NativeTcpServer& tcp_server,
                          ITcpServerHandler& handler,
                          uint16_t port,
                          const char* label) {
    TcpListenOptions options;
    options.host = "0.0.0.0";
    options.port = port;
    options.reuse_address = true;

    if (!tcp_server.listen(options, handler)) {
        print_text(label);
        print_text(" listen failed\n");
        return false;
    }

    print_text(label);
    print_text(" listening on port ");
    print_number(tcp_server.port());
    print_text("\n");
    return true;
}

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

    if (!listen_server(signalk_server, signalk_handler, signalk_port, "Signal K")) {
        return false;
    }
    if (!listen_server(nmea_server, nmea_handler, nmea_port, "NMEA 0183")) {
        signalk_server.close();
        return false;
    }

    print_text("MiniSignalK bridge ready\n");
    print_text("connect Signal K clients to port ");
    print_number(signalk_port);
    print_text("\n");
    print_text("send NMEA MWV to port ");
    print_number(nmea_port);
    print_text(", for example: $IIMWV,045.0,R,12.3,N,A*00\n");
    return true;
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    if (!setup_example()) {
        print_text("MiniSignalK setup failed\n");
        setup_failed = true;
    }
}

void loop() {
    if (setup_failed) {
        print_text("MiniSignalK setup failed\n");
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
