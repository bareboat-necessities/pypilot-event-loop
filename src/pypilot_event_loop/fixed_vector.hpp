#pragma once

#include <stddef.h>

namespace pypilot_event_loop {

template<typename T, size_t Capacity>
class FixedVector final {
public:
    bool push_back(const T& value) {
        if (size_ >= Capacity) {
            return false;
        }
        items_[size_++] = value;
        return true;
    }

    void clear() { size_ = 0; }
    size_t size() const { return size_; }
    size_t capacity() const { return Capacity; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ >= Capacity; }

    T& operator[](size_t index) { return items_[index]; }
    const T& operator[](size_t index) const { return items_[index]; }

private:
    T items_[Capacity]{};
    size_t size_ = 0;
};

} // namespace pypilot_event_loop
