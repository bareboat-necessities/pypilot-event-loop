#pragma once

#include <stddef.h>
#include <stdint.h>

namespace async_event_loop {

struct TcpListenOptions {
    const char* host = "127.0.0.1";
    uint16_t port = 0;
    int backlog = -1;
    bool reuse_address = true;
    bool close_on_free = true;
};

struct TcpConnectOptions {
    const char* host = "127.0.0.1";
    uint16_t port = 0;
};

struct TcpTimeoutOptions {
    uint32_t read_timeout_ms = 0;
    uint32_t write_timeout_ms = 0;
};

struct TcpWatermarkOptions {
    size_t read_low = 0;
    size_t read_high = 0;
    size_t write_low = 0;
    size_t write_high = 0;
};

struct TcpPeerInfo {
    char host[64]{};
    uint16_t port = 0;
};

class ITcpConnection {
public:
    virtual ~ITcpConnection() = default;

    virtual bool valid() const = 0;
    virtual void close() = 0;

    virtual const TcpPeerInfo& peer() const = 0;
    virtual size_t input_size() const = 0;
    virtual size_t output_size() const = 0;

    virtual int read(uint8_t* dst, size_t max_len) = 0;
    virtual int write(const uint8_t* src, size_t len) = 0;

    virtual bool peek(uint8_t* dst, size_t len) = 0;
    virtual bool read_exact(uint8_t* dst, size_t len) = 0;
    virtual bool read_line(char* dst, size_t max_len, bool strip_cr = true) = 0;

    virtual bool set_timeouts(const TcpTimeoutOptions& options) {
        (void)options;
        return false;
    }

    virtual bool set_watermarks(const TcpWatermarkOptions& options) {
        (void)options;
        return false;
    }
};

class ITcpServerHandler {
public:
    virtual ~ITcpServerHandler() = default;

    virtual void on_accept(ITcpConnection& connection, const TcpPeerInfo& peer) {
        (void)connection;
        (void)peer;
    }

    virtual void on_data(ITcpConnection& connection) {
        (void)connection;
    }

    virtual void on_write_ready(ITcpConnection& connection) {
        (void)connection;
    }

    virtual void on_close(ITcpConnection& connection) {
        (void)connection;
    }

    virtual void on_error(ITcpConnection& connection, int error_code) {
        (void)connection;
        (void)error_code;
    }

    virtual void on_listener_error(int error_code) {
        (void)error_code;
    }
};

class ITcpClientHandler {
public:
    virtual ~ITcpClientHandler() = default;

    virtual void on_connect(ITcpConnection& connection, const TcpPeerInfo& peer) {
        (void)connection;
        (void)peer;
    }

    virtual void on_data(ITcpConnection& connection) {
        (void)connection;
    }

    virtual void on_write_ready(ITcpConnection& connection) {
        (void)connection;
    }

    virtual void on_close(ITcpConnection& connection) {
        (void)connection;
    }

    virtual void on_error(int error_code) {
        (void)error_code;
    }
};

} // namespace async_event_loop
