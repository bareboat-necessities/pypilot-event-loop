#pragma once

#include <new>
#include <stddef.h>
#include <stdint.h>

#include "byte_stream.hpp"
#include "task.hpp"

namespace pypilot_event_loop {

struct LineView {
    constexpr LineView() = default;
    constexpr LineView(const char* data_value, size_t size_value)
        : data(data_value), size(size_value) {}

    const char* data = nullptr;
    size_t size = 0;
};

struct FrameView {
    constexpr FrameView() = default;
    constexpr FrameView(const uint8_t* data_value, size_t size_value)
        : data(data_value), size(size_value) {}

    const uint8_t* data = nullptr;
    size_t size = 0;
};

struct LineProtocolOptions {
    constexpr LineProtocolOptions() = default;
    constexpr LineProtocolOptions(char delimiter_value, bool strip_cr_value = true, bool drop_overflow_value = true)
        : delimiter(delimiter_value), strip_cr(strip_cr_value), drop_overflow(drop_overflow_value) {}

    char delimiter = '\n';
    bool strip_cr = true;
    bool drop_overflow = true;
};

struct FixedFrameProtocolOptions {
    constexpr FixedFrameProtocolOptions() = default;
    explicit constexpr FixedFrameProtocolOptions(size_t frame_size_value)
        : frame_size(frame_size_value) {}

    size_t frame_size = 0;
};

using PayloadSizeFromHeaderFn = size_t (*)(const uint8_t* header, size_t header_size);

struct HeaderPayloadProtocolOptions {
    constexpr HeaderPayloadProtocolOptions() = default;
    constexpr HeaderPayloadProtocolOptions(size_t header_size_value,
                                           size_t max_payload_size_value,
                                           PayloadSizeFromHeaderFn payload_size_from_header_value)
        : header_size(header_size_value),
          max_payload_size(max_payload_size_value),
          payload_size_from_header(payload_size_from_header_value) {}

    size_t header_size = 0;
    size_t max_payload_size = 0;
    PayloadSizeFromHeaderFn payload_size_from_header = nullptr;
};

struct ProtocolReaderStats {
    uint32_t messages = 0;
    uint32_t bytes = 0;
    uint32_t overflows = 0;
    uint32_t protocol_errors = 0;
};

template<size_t StorageSize, typename ViewType>
class ProtocolCallbackStorage {
public:
    ProtocolCallbackStorage() = default;
    ProtocolCallbackStorage(const ProtocolCallbackStorage&) = delete;
    ProtocolCallbackStorage& operator=(const ProtocolCallbackStorage&) = delete;
    ~ProtocolCallbackStorage() { clear(); }

    template<typename Callable>
    bool set(Callable callable) {
        static_assert(sizeof(Callable) <= StorageSize, "protocol callback is too large for storage");
        clear();
        new (storage_) Callable(callable);
        invoke_ = [](void* storage, const ViewType& view) {
            (*static_cast<Callable*>(storage))(view);
        };
        destroy_ = [](void* storage) {
            static_cast<Callable*>(storage)->~Callable();
        };
        active_ = true;
        return true;
    }

    void invoke(const ViewType& view) {
        if (active_ && invoke_) {
            invoke_(storage_, view);
        }
    }

    bool active() const { return active_; }

    void clear() {
        if (active_ && destroy_) {
            destroy_(storage_);
        }
        active_ = false;
        invoke_ = nullptr;
        destroy_ = nullptr;
    }

private:
    alignas(void*) unsigned char storage_[StorageSize]{};
    void (*invoke_)(void*, const ViewType&) = nullptr;
    void (*destroy_)(void*) = nullptr;
    bool active_ = false;
};

template<size_t BufferSize = 256, size_t CallbackStorageSize = 64>
class LineProtocolReader final : public IRuntimeTask {
public:
    explicit LineProtocolReader(IByteStream& stream,
                                LineProtocolOptions options = LineProtocolOptions{})
        : stream_(stream), options_(options) {}

    template<typename Callable>
    LineProtocolReader(IByteStream& stream,
                       LineProtocolOptions options,
                       Callable callable)
        : stream_(stream), options_(options) {
        on_data_ready(callable);
    }

    template<typename Callable>
    bool on_data_ready(Callable callable) {
        return callback_.set(callable);
    }

    IByteStream& stream() { return stream_; }
    const ProtocolReaderStats& stats() const { return stats_; }

    void poll(uint64_t) override {
        uint8_t byte = 0;
        while (stream_.readable()) {
            const int n = stream_.read(&byte, 1);
            if (n <= 0) {
                break;
            }
            ++stats_.bytes;
            consume(static_cast<char>(byte));
        }
    }

private:
    void consume(char c) {
        if (c == options_.delimiter) {
            emit_line();
            return;
        }

        if (size_ >= BufferSize) {
            ++stats_.overflows;
            if (options_.drop_overflow) {
                size_ = 0;
            }
            return;
        }

        buffer_[size_++] = c;
    }

    void emit_line() {
        size_t len = size_;
        if (options_.strip_cr && len > 0 && buffer_[len - 1] == '\r') {
            --len;
        }
        LineView view;
        view.data = buffer_;
        view.size = len;
        callback_.invoke(view);
        ++stats_.messages;
        size_ = 0;
    }

    IByteStream& stream_;
    LineProtocolOptions options_;
    char buffer_[BufferSize]{};
    size_t size_ = 0;
    ProtocolReaderStats stats_;
    ProtocolCallbackStorage<CallbackStorageSize, LineView> callback_;
};

template<size_t BufferSize = 256, size_t CallbackStorageSize = 64>
class FixedFrameProtocolReader final : public IRuntimeTask {
public:
    explicit FixedFrameProtocolReader(IByteStream& stream,
                                      FixedFrameProtocolOptions options)
        : stream_(stream), options_(options) {}

    template<typename Callable>
    FixedFrameProtocolReader(IByteStream& stream,
                             FixedFrameProtocolOptions options,
                             Callable callable)
        : stream_(stream), options_(options) {
        on_data_ready(callable);
    }

    template<typename Callable>
    bool on_data_ready(Callable callable) {
        return callback_.set(callable);
    }

    IByteStream& stream() { return stream_; }
    const ProtocolReaderStats& stats() const { return stats_; }

    void poll(uint64_t) override {
        if (options_.frame_size == 0 || options_.frame_size > BufferSize) {
            ++stats_.protocol_errors;
            return;
        }

        uint8_t byte = 0;
        while (stream_.readable()) {
            const int n = stream_.read(&byte, 1);
            if (n <= 0) {
                break;
            }
            ++stats_.bytes;
            buffer_[size_++] = byte;
            if (size_ == options_.frame_size) {
                FrameView view;
                view.data = buffer_;
                view.size = size_;
                callback_.invoke(view);
                ++stats_.messages;
                size_ = 0;
            }
        }
    }

private:
    IByteStream& stream_;
    FixedFrameProtocolOptions options_;
    uint8_t buffer_[BufferSize]{};
    size_t size_ = 0;
    ProtocolReaderStats stats_;
    ProtocolCallbackStorage<CallbackStorageSize, FrameView> callback_;
};

template<size_t BufferSize = 512, size_t CallbackStorageSize = 64>
class HeaderPayloadProtocolReader final : public IRuntimeTask {
public:
    explicit HeaderPayloadProtocolReader(IByteStream& stream,
                                         HeaderPayloadProtocolOptions options)
        : stream_(stream), options_(options) {}

    template<typename Callable>
    HeaderPayloadProtocolReader(IByteStream& stream,
                                HeaderPayloadProtocolOptions options,
                                Callable callable)
        : stream_(stream), options_(options) {
        on_data_ready(callable);
    }

    template<typename Callable>
    bool on_data_ready(Callable callable) {
        return callback_.set(callable);
    }

    IByteStream& stream() { return stream_; }
    const ProtocolReaderStats& stats() const { return stats_; }

    void poll(uint64_t) override {
        if (!valid_options()) {
            ++stats_.protocol_errors;
            return;
        }

        uint8_t byte = 0;
        while (stream_.readable()) {
            const int n = stream_.read(&byte, 1);
            if (n <= 0) {
                break;
            }
            ++stats_.bytes;
            if (!append(byte)) {
                continue;
            }
            try_emit();
        }
    }

private:
    bool valid_options() const {
        return options_.header_size != 0 &&
               options_.payload_size_from_header != nullptr &&
               options_.header_size <= BufferSize &&
               options_.max_payload_size <= BufferSize &&
               options_.header_size + options_.max_payload_size <= BufferSize;
    }

    bool append(uint8_t byte) {
        if (size_ >= BufferSize) {
            ++stats_.overflows;
            reset();
            return false;
        }
        buffer_[size_++] = byte;
        return true;
    }

    void try_emit() {
        if (size_ < options_.header_size) {
            return;
        }

        if (!have_expected_size_) {
            const size_t payload = options_.payload_size_from_header(buffer_, options_.header_size);
            if (payload > options_.max_payload_size || options_.header_size + payload > BufferSize) {
                ++stats_.protocol_errors;
                reset();
                return;
            }
            expected_size_ = options_.header_size + payload;
            have_expected_size_ = true;
        }

        if (size_ >= expected_size_) {
            FrameView view;
            view.data = buffer_;
            view.size = expected_size_;
            callback_.invoke(view);
            ++stats_.messages;

            const size_t remaining = size_ - expected_size_;
            for (size_t i = 0; i < remaining; ++i) {
                buffer_[i] = buffer_[expected_size_ + i];
            }
            size_ = remaining;
            expected_size_ = 0;
            have_expected_size_ = false;
            if (size_ >= options_.header_size) {
                try_emit();
            }
        }
    }

    void reset() {
        size_ = 0;
        expected_size_ = 0;
        have_expected_size_ = false;
    }

    IByteStream& stream_;
    HeaderPayloadProtocolOptions options_;
    uint8_t buffer_[BufferSize]{};
    size_t size_ = 0;
    size_t expected_size_ = 0;
    bool have_expected_size_ = false;
    ProtocolReaderStats stats_;
    ProtocolCallbackStorage<CallbackStorageSize, FrameView> callback_;
};

} // namespace pypilot_event_loop
