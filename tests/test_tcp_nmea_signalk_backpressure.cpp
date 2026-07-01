#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "async_event_loop.hpp"

namespace {

template<typename Real = float>
struct DataValue {
    Real value = Real(0);
    uint64_t last_update_us = 0;
    bool valid = false;

    void set(Real new_value, uint64_t now_us) {
        value = new_value;
        last_update_us = now_us;
        valid = true;
    }
};

template<typename Real = float>
struct DataModel {
    DataValue<Real> apparent_wind_direction_rad;
    DataValue<Real> apparent_wind_speed_m_s;
};

using Real = float;

constexpr Real knots_to_m_s = Real(0.514444);
constexpr Real deg_to_rad = Real(3.14159265358979323846) / Real(180);
constexpr int max_sentences_before_disconnect = 8192;
constexpr size_t backpressure_limit_bytes = 1024;

bool view_contains(async_event_loop::JsonView view, const char* needle) {
    const size_t needle_len = std::strlen(needle);
    if (!view.data || !needle || needle_len == 0 || view.size < needle_len) {
        return false;
    }
    for (size_t i = 0; i + needle_len <= view.size; ++i) {
        if (std::memcmp(view.data + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

struct JsonCapture {
    async_event_loop::MemoryByteStream<8192> stream;
    int messages = 0;
    bool saw_angle_path = false;
    bool saw_speed_path = false;
    bool saw_angle_value = false;
    bool saw_speed_value = false;
    async_event_loop::JsonProtocolReader<1024> reader;

    JsonCapture()
        : reader(stream, async_event_loop::JsonProtocolOptions{}, [this](async_event_loop::JsonView json) {
              ++messages;
              saw_angle_path = saw_angle_path || view_contains(json, "environment.wind.angleApparent");
              saw_speed_path = saw_speed_path || view_contains(json, "environment.wind.speedApparent");
              saw_angle_value = saw_angle_value || view_contains(json, "\"value\":0.785398");
              saw_speed_value = saw_speed_value || view_contains(json, "\"value\":6.327661");
          }) {}
};

class NmeaTokenizer {
public:
    static constexpr int max_fields = 8;

    bool tokenize(char* text) {
        reset();
        if (!text) {
            return false;
        }

        char* save = nullptr;
        for (char* token = strtok_r(text, ",", &save);
             token && field_count_ < max_fields;
             token = strtok_r(nullptr, ",", &save)) {
            fields_[field_count_++] = token;
        }
        return field_count_ > 0;
    }

    int field_count() const { return field_count_; }

    const char* field(int index) const {
        if (index < 0 || index >= field_count_) {
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

bool parse_mwv(const NmeaTokenizer& tokenizer, DataModel<Real>& model, uint64_t now_us) {
    if (tokenizer.field_count() < 6 || std::strlen(tokenizer.field(0)) < 6) {
        return false;
    }
    const char* sentence = tokenizer.field(0) + std::strlen(tokenizer.field(0)) - 3;
    if (std::strcmp(sentence, "MWV") != 0 || std::strcmp(tokenizer.field(2), "R") != 0) {
        return false;
    }
    if (tokenizer.field(5)[0] != 'A') {
        return false;
    }

    const char* angle_field = tokenizer.field(1);
    const char* speed_field = tokenizer.field(3);
    char* end_angle = nullptr;
    char* end_speed = nullptr;
    const Real angle_deg = static_cast<Real>(std::strtod(angle_field, &end_angle));
    const Real speed_value = static_cast<Real>(std::strtod(speed_field, &end_speed));
    if (end_angle == angle_field || end_speed == speed_field) {
        return false;
    }

    Real factor = Real(1);
    if (std::strcmp(tokenizer.field(4), "N") == 0) {
        factor = knots_to_m_s;
    } else if (std::strcmp(tokenizer.field(4), "M") == 0) {
        factor = Real(1);
    } else if (std::strcmp(tokenizer.field(4), "K") == 0) {
        factor = Real(1000) / Real(3600);
    } else {
        return false;
    }

    model.apparent_wind_direction_rad.set(angle_deg * deg_to_rad, now_us);
    model.apparent_wind_speed_m_s.set(speed_value * factor, now_us);
    return true;
}

struct WindSignalKServer final : public async_event_loop::ITcpLineServerHandler {
    explicit WindSignalKServer(async_event_loop::EventLoop<>& loop) : event_loop(loop) {}

    async_event_loop::EventLoop<>& event_loop;
    DataModel<Real> data_model;
    async_event_loop::TcpConnectionRegistry<8> connections;
    int accepted = 0;
    int nmea_messages = 0;
    int json_broadcasts = 0;
    int backpressure_disconnects = 0;
    bool backpressure_error_logged = false;
    size_t max_first_client_output = 0;

    void on_accept(async_event_loop::ITcpConnection& connection,
                   const async_event_loop::TcpPeerInfo& peer) override {
        (void)peer;
        assert(connections.add(connection));
        ++accepted;
    }

    void on_line(async_event_loop::ITcpConnection& connection, async_event_loop::LineView line) override {
        (void)connection;

        char text[128];
        async_event_loop::line_to_cstr(line, text);

        NmeaTokenizer tokenizer;
        if (!tokenizer.tokenize(text)) {
            return;
        }

        const uint64_t now_us = event_loop.clock().micros();
        if (!parse_mwv(tokenizer, data_model, now_us)) {
            return;
        }
        assert(data_model.apparent_wind_direction_rad.valid);
        assert(data_model.apparent_wind_speed_m_s.valid);
        assert(data_model.apparent_wind_direction_rad.last_update_us == now_us);
        assert(data_model.apparent_wind_speed_m_s.last_update_us == now_us);
        ++nmea_messages;

        char json[512];
        const int len = std::snprintf(
            json,
            sizeof(json),
            "{\"updates\":[{\"source\":{\"label\":\"pypilot-event-loop-test\"},"
            "\"values\":[{\"path\":\"environment.wind.angleApparent\",\"value\":%.6f},"
            "{\"path\":\"environment.wind.speedApparent\",\"value\":%.6f}]}]}\n",
            static_cast<double>(data_model.apparent_wind_direction_rad.value),
            static_cast<double>(data_model.apparent_wind_speed_m_s.value));
        assert(len > 0 && len < static_cast<int>(sizeof(json)));

        connections.for_each([&](async_event_loop::ITcpConnection& peer) {
            const int written = peer.write(reinterpret_cast<const uint8_t*>(json), static_cast<size_t>(len));
            assert(written == len);
        });
        ++json_broadcasts;
    }

    void on_backpressure(async_event_loop::ITcpConnection& connection,
                         const async_event_loop::TcpBackpressureInfo& info) override {
        std::fprintf(stderr,
                     "backpressure: disconnecting client with %zu queued bytes\n",
                     info.pending_bytes);
        backpressure_error_logged = true;
        ++backpressure_disconnects;
        if (connections.at(0) == &connection && info.pending_bytes > max_first_client_output) {
            max_first_client_output = info.pending_bytes;
        }
        connections.remove(connection);
    }

    void on_close(async_event_loop::ITcpConnection& connection) override {
        connections.remove(connection);
    }

    void on_error(async_event_loop::ITcpConnection& connection, int error_code) override {
        std::fprintf(stderr, "connection error: %d\n", error_code);
        connections.remove(connection);
    }
};

int connect_client(uint16_t port, bool small_receive_buffer) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    if (small_receive_buffer) {
        int rcvbuf = 1024;
        assert(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) == 0);
#ifdef TCP_WINDOW_CLAMP
        int window_clamp = 1024;
        assert(setsockopt(fd, IPPROTO_TCP, TCP_WINDOW_CLAMP, &window_clamp, sizeof(window_clamp)) == 0);
#endif
    }
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(connect(fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) == 0);
    const int flags = fcntl(fd, F_GETFL, 0);
    assert(flags >= 0);
    assert(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
    return fd;
}

void pump(async_event_loop::EventLoop<>& event_loop, int iterations = 1) {
    for (int i = 0; i < iterations; ++i) {
        event_loop.run_once();
    }
}

void drain_plain_client(int fd) {
    uint8_t buf[1024];
    for (;;) {
        const ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            continue;
        }
        if (n == 0) {
            return;
        }
        assert(errno == EAGAIN || errno == EWOULDBLOCK);
        return;
    }
}

void drain_json_client(int fd, JsonCapture& capture) {
    uint8_t buf[1024];
    for (;;) {
        const ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            const int written = capture.stream.write(buf, static_cast<size_t>(n));
            assert(written == static_cast<int>(n));
            capture.reader.poll(0);
            continue;
        }
        if (n == 0) {
            return;
        }
        assert(errno == EAGAIN || errno == EWOULDBLOCK);
        return;
    }
}

void write_all(async_event_loop::EventLoop<>& event_loop, int fd, const char* text) {
    size_t written = 0;
    const size_t len = std::strlen(text);
    while (written < len) {
        const ssize_t n = write(fd, text + written, len - written);
        if (n > 0) {
            written += static_cast<size_t>(n);
            continue;
        }
        assert(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
        pump(event_loop, 4);
    }
}

void wait_for_accepts(async_event_loop::EventLoop<>& event_loop,
                      WindSignalKServer& app,
                      int expected) {
    for (int i = 0; i < 500 && app.accepted < expected; ++i) {
        pump(event_loop);
    }
    assert(app.accepted == expected);
    assert(app.connections.size() == static_cast<size_t>(expected));
}

void wait_for_json(async_event_loop::EventLoop<>& event_loop,
                   int fast_fd,
                   JsonCapture& capture,
                   int expected_messages) {
    for (int i = 0; i < 5000 && capture.messages < expected_messages; ++i) {
        pump(event_loop);
        drain_json_client(fast_fd, capture);
    }
    assert(capture.messages >= expected_messages);
}

} // namespace

int main() {
    async_event_loop::EventLoop<> event_loop;
    assert(event_loop.valid());

    WindSignalKServer app(event_loop);
    async_event_loop::TcpLineServerOptions line_options;
    line_options.backpressure.enabled = true;
    line_options.backpressure.high_water_bytes = backpressure_limit_bytes;
    line_options.backpressure.low_water_bytes = 256;
    line_options.backpressure.max_over_high_polls = 1;
    line_options.backpressure.close_on_limit = true;
    async_event_loop::TcpLineServerHandler<160, 8> line_server(app, line_options);
    async_event_loop::NativeTcpServer server(event_loop.scheduler());

    async_event_loop::TcpListenOptions options;
    options.host = "127.0.0.1";
    options.port = 0;
    options.reuse_address = true;
    assert(server.listen(options, line_server));
    assert(server.port() != 0);

    const int slow_fd = connect_client(server.port(), true);
    const int fast_fd = connect_client(server.port(), false);
    const int nmea_fd = connect_client(server.port(), false);
    wait_for_accepts(event_loop, app, 3);
    assert(server.connection_count() == 3);

    JsonCapture fast_json;
    const char sentence[] = "$IIMWV,045.0,R,12.3,N,A*00\r\n";

    write_all(event_loop, nmea_fd, sentence);
    wait_for_json(event_loop, fast_fd, fast_json, 1);
    drain_plain_client(nmea_fd);
    assert(app.nmea_messages == 1);
    assert(app.json_broadcasts == 1);
    assert(app.data_model.apparent_wind_direction_rad.valid);
    assert(app.data_model.apparent_wind_speed_m_s.valid);
    assert(app.data_model.apparent_wind_direction_rad.last_update_us ==
           app.data_model.apparent_wind_speed_m_s.last_update_us);
    assert(fast_json.saw_angle_path);
    assert(fast_json.saw_speed_path);
    assert(fast_json.saw_angle_value);
    assert(fast_json.saw_speed_value);

    const uint64_t first_update_us = app.data_model.apparent_wind_direction_rad.last_update_us;
    for (int i = 0; i < max_sentences_before_disconnect && app.backpressure_disconnects == 0; ++i) {
        write_all(event_loop, nmea_fd, sentence);
        pump(event_loop, 2);
        drain_json_client(fast_fd, fast_json);
        drain_plain_client(nmea_fd);
    }
    assert(app.backpressure_disconnects == 1);
    assert(app.backpressure_error_logged);
    assert(app.max_first_client_output > backpressure_limit_bytes);
    assert(app.connections.at(0) == nullptr);
    assert(app.connections.at(1) != nullptr);
    assert(app.connections.at(2) != nullptr);
    assert(app.data_model.apparent_wind_direction_rad.last_update_us >= first_update_us);
    assert(app.data_model.apparent_wind_speed_m_s.last_update_us >= first_update_us);

    pump(event_loop, 100);
    assert(server.connection_count() == 2);

    const int messages_before = fast_json.messages;
    const int broadcasts_before = app.json_broadcasts;
    const uint64_t before_continue_update_us = app.data_model.apparent_wind_speed_m_s.last_update_us;
    for (int i = 0; i < 8; ++i) {
        write_all(event_loop, nmea_fd, sentence);
        pump(event_loop, 2);
        drain_json_client(fast_fd, fast_json);
        drain_plain_client(nmea_fd);
    }
    assert(app.json_broadcasts >= broadcasts_before + 8);
    assert(app.data_model.apparent_wind_direction_rad.last_update_us >= before_continue_update_us);
    assert(app.data_model.apparent_wind_speed_m_s.last_update_us >= before_continue_update_us);
    wait_for_json(event_loop, fast_fd, fast_json, messages_before + 8);

    close(nmea_fd);
    close(fast_fd);
    close(slow_fd);
    pump(event_loop, 100);
    server.close();
    return 0;
}
