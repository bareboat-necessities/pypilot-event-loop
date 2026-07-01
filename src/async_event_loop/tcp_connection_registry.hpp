#pragma once

#include <stddef.h>

#include "tcp.hpp"

namespace async_event_loop {

template<size_t MaxConnections>
class TcpConnectionRegistry final {
public:
    static constexpr size_t capacity() { return MaxConnections; }

    bool add(ITcpConnection& connection) {
        const int existing = index_of(connection);
        if (existing >= 0) {
            return true;
        }
        for (size_t i = 0; i < MaxConnections; ++i) {
            if (!connections_[i]) {
                connections_[i] = &connection;
                ++count_;
                return true;
            }
        }
        return false;
    }

    bool remove(ITcpConnection& connection) {
        const int index = index_of(connection);
        if (index < 0) {
            return false;
        }
        connections_[static_cast<size_t>(index)] = nullptr;
        if (count_ > 0) {
            --count_;
        }
        return true;
    }

    void clear() {
        for (size_t i = 0; i < MaxConnections; ++i) {
            connections_[i] = nullptr;
        }
        count_ = 0;
    }

    void prune_invalid() {
        for (size_t i = 0; i < MaxConnections; ++i) {
            ITcpConnection* connection = connections_[i];
            if (connection && !connection->valid()) {
                connections_[i] = nullptr;
                if (count_ > 0) {
                    --count_;
                }
            }
        }
    }

    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    bool full() const { return count_ >= MaxConnections; }

    bool contains(ITcpConnection& connection) const {
        return index_of(connection) >= 0;
    }

    int index_of(ITcpConnection& connection) const {
        for (size_t i = 0; i < MaxConnections; ++i) {
            if (connections_[i] == &connection) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    ITcpConnection* at(size_t index) {
        return index < MaxConnections ? connections_[index] : nullptr;
    }

    const ITcpConnection* at(size_t index) const {
        return index < MaxConnections ? connections_[index] : nullptr;
    }

    ITcpConnection* operator[](size_t index) { return at(index); }
    const ITcpConnection* operator[](size_t index) const { return at(index); }

    template<typename Callback>
    void for_each(Callback callback) {
        for (size_t i = 0; i < MaxConnections; ++i) {
            ITcpConnection* connection = connections_[i];
            if (connection && connection->valid()) {
                callback(*connection);
            }
        }
    }

private:
    ITcpConnection* connections_[MaxConnections]{};
    size_t count_ = 0;
};

} // namespace async_event_loop
