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
#endif

#include <stdint.h>

#include <async_event_loop.hpp>
#include "../support/mini_signalk_support.hpp"

using namespace async_event_loop;
using async_event_loop_examples::DataModel;
using async_event_loop_examples::NmeaTokenizer;
using async_event_loop_examples::Real;
using async_event_loop_examples::format_signalk_wind_update;
using async_event_loop_examples::parse_mwv;
using async_event_loop_examples::print_float;
using async_event_loop_examples::print_number;
using async_event_loop_examples::print_size;
using async_event_loop_examples::print_text;

// Mini bridge layout:
//   * Signal K clients connect to signalk_port and receive newline-delimited JSON updates.
//   * NMEA 0183 producers connect to nmea_port and send line-delimited MWV sentences.
// The two ports are intentionally separate so a stalled Signal K subscriber cannot block
// or be confused with an NMEA input source.
static constexpr uint16_t signalk_port = 20223;
static constexpr uint16_t nmea_port = 20224;

static EventLoop<> event_loop;
static NativeTcpServer signalk_server(event_loop.scheduler());
static NativeTcpServer nmea_server(event_loop.scheduler());

#if defined(ARDUINO)
static bool setup_failed = false;
#endif

static TcpLineServerOptions make_signalk_options() {
    TcpLineServerOptions options;
#if defined(ARDUINO)
    // Embedded WiFi clients have much smaller TCP output queues, so use the
    // library's embedded backpressure threshold.
    options.backpressure = TcpBackpressureOptions::embedded_default();
#else
    // Linux/libevent can buffer more data, so use the server-side default.
    options.backpressure = TcpBackpressureOptions::server_default();
#endif
    return options;
}

// Signal K side: accepts subscribers and broadcasts generated Signal K JSON.
// It does not parse Signal K input yet; received lines are ignored in this minimal example.
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

        // The line handler will close the connection after this callback when
        // close_on_limit is true. The application registry is updated here so
        // future broadcasts stop targeting the stalled subscriber immediately.
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

    void broadcast_wind_update(const DataModel<Real>& data_model) {
        char json[512];
        const int len = format_signalk_wind_update(data_model, json, sizeof(json));
        if (len <= 0 || len >= static_cast<int>(sizeof(json))) {
            return;
        }

        // Writes are queued by the TCP backend. The library backpressure monitor
        // detects subscribers whose queued output grows beyond the configured limit.
        clients.for_each([&](ITcpConnection& peer) {
            peer.write(reinterpret_cast<const uint8_t*>(json), static_cast<size_t>(len));
        });
        ++broadcasts;
    }
};

// NMEA side: accepts line-delimited NMEA sources, parses MWV, and publishes
// the converted values through the Signal K side.
struct NmeaCallbacks final : public ITcpLineServerHandler {
    explicit NmeaCallbacks(SignalKCallbacks& sink) : signalk(sink) {}

    SignalKCallbacks& signalk;
    TcpConnectionRegistry<4> sources;
    DataModel<Real> data_model;
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

        // TcpLineServerHandler strips the line framing; copy it into a mutable
        // buffer because NmeaTokenizer splits fields in place.
        char text[160];
        line_to_cstr(line, text);

        NmeaTokenizer tokenizer;
        if (!tokenizer.tokenize(text)) {
            return;
        }

        if (!parse_mwv(tokenizer, data_model, event_loop.clock().micros())) {
            return;
        }

        ++updates;
        print_text("NMEA MWV update angle_rad=");
        print_float(data_model.apparent_wind_direction_rad.value);
        print_text(" speed_m_s=");
        print_float(data_model.apparent_wind_speed_m_s.value);
        print_text(" SignalK clients=");
        print_size(signalk.clients.size());
        print_text("\n");

        signalk.broadcast_wind_update(data_model);
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

    // Start both listeners. If the second listener cannot start, close the first
    // so the example never runs in a half-bridged state.
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
