#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pypilot_event_loop/task.hpp"
#include "pypilot_event_loop/tcp.hpp"
#include "pypilot_event_loop_linux/libevent_loop.hpp"

namespace pypilot_event_loop {

class LinuxTcpClientConnection final : public ITcpConnection {
public:
    ~LinuxTcpClientConnection() override { close(); }

    bool attach(int fd, const TcpPeerInfo& peer) {
        close();
        fd_ = fd;
        peer_ = peer;
        set_nonblocking();
        return fd_ >= 0;
    }

    bool valid() const override { return fd_ >= 0; }
    void close() override {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        line_size_ = 0;
    }

    const TcpPeerInfo& peer() const override { return peer_; }
    size_t input_size() const override { return fd_ >= 0 ? 1u : 0u; }
    size_t output_size() const override { return 0; }

    int read(uint8_t* dst, size_t max_len) override {
        if (fd_ < 0 || !dst || max_len == 0) {
            return 0;
        }
        const ssize_t n = ::read(fd_, dst, max_len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return 0;
            }
            return -1;
        }
        return static_cast<int>(n);
    }

    int write(const uint8_t* src, size_t len) override {
        if (fd_ < 0 || !src || len == 0) {
            return 0;
        }
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
        const ssize_t n = ::send(fd_, src, len, flags);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return 0;
            }
            return -1;
        }
        return static_cast<int>(n);
    }

    bool peek(uint8_t* dst, size_t len) override {
        if (fd_ < 0 || !dst || len == 0) {
            return false;
        }
        const ssize_t n = ::recv(fd_, dst, len, MSG_PEEK);
        return n == static_cast<ssize_t>(len);
    }

    bool read_exact(uint8_t* dst, size_t len) override {
        if (!dst) {
            return false;
        }
        size_t done = 0;
        while (done < len) {
            const int n = read(dst + done, len - done);
            if (n <= 0) {
                return false;
            }
            done += static_cast<size_t>(n);
        }
        return true;
    }

    bool read_line(char* dst, size_t max_len, bool strip_cr = true) override {
        if (!dst || max_len == 0) {
            return false;
        }
        uint8_t byte = 0;
        while (true) {
            const int n = read(&byte, 1);
            if (n <= 0) {
                return false;
            }
            if (byte == '\n') {
                size_t len = line_size_;
                if (strip_cr && len > 0 && line_[len - 1] == '\r') {
                    --len;
                }
                const size_t copy = len < max_len - 1 ? len : max_len - 1;
                memcpy(dst, line_, copy);
                dst[copy] = '\0';
                line_size_ = 0;
                return true;
            }
            if (line_size_ < sizeof(line_)) {
                line_[line_size_++] = static_cast<char>(byte);
            } else {
                line_size_ = 0;
            }
        }
    }

    int native_fd() const { return fd_; }

private:
    void set_nonblocking() {
        if (fd_ < 0) {
            return;
        }
        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }
    }

    int fd_ = -1;
    TcpPeerInfo peer_;
    char line_[256]{};
    size_t line_size_ = 0;
};

class LinuxTcpClient final : public IRuntimeTask {
public:
    explicit LinuxTcpClient(LinuxLibeventLoop& loop) : loop_(loop) {}
    ~LinuxTcpClient() override { close(); }

    bool connect(const TcpConnectOptions& options, ITcpClientHandler& handler) {
        close();
        if (!options.host || options.port == 0) {
            handler.on_error(EINVAL);
            return false;
        }

        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            handler.on_error(errno);
            return false;
        }

        sockaddr_in sin{};
        sin.sin_family = AF_INET;
        sin.sin_port = htons(options.port);
        if (inet_pton(AF_INET, options.host, &sin.sin_addr) != 1) {
            ::close(fd);
            handler.on_error(EINVAL);
            return false;
        }

        if (::connect(fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)) != 0) {
            const int err = errno;
            ::close(fd);
            handler.on_error(err);
            return false;
        }

        TcpPeerInfo peer{};
        snprintf(peer.host, sizeof(peer.host), "%s", options.host);
        peer.port = options.port;
        connection_.attach(fd, peer);
        handler_ = &handler;
        if (!loop_.add_readable_fd(connection_.native_fd(), *this)) {
            const int err = errno ? errno : EINVAL;
            connection_.close();
            handler_ = nullptr;
            handler.on_error(err);
            return false;
        }
        registered_fd_ = connection_.native_fd();
        handler_->on_connect(connection_, connection_.peer());
        return true;
    }

    void close() {
        unregister_fd();
        connection_.close();
        handler_ = nullptr;
    }

    bool valid() const { return connection_.valid(); }
    ITcpConnection& connection() { return connection_; }

    void poll(uint64_t) override {
        if (!handler_ || !connection_.valid()) {
            return;
        }

        uint8_t probe = 0;
        const int fd = connection_.native_fd();
        const ssize_t n = ::recv(fd, &probe, 1, MSG_PEEK);
        if (n > 0) {
            handler_->on_data(connection_);
            return;
        }
        if (n == 0) {
            ITcpClientHandler* handler = handler_;
            handler->on_close(connection_);
            close();
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return;
        }

        const int err = errno;
        ITcpClientHandler* handler = handler_;
        handler->on_error(err);
        close();
    }

private:
    void unregister_fd() {
        if (registered_fd_ >= 0) {
            loop_.remove_fd(registered_fd_, *this);
            registered_fd_ = -1;
        }
    }

    LinuxLibeventLoop& loop_;
    ITcpClientHandler* handler_ = nullptr;
    LinuxTcpClientConnection connection_;
    int registered_fd_ = -1;
};

} // namespace pypilot_event_loop
