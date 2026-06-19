#pragma once

#include <stddef.h>

namespace pypilot_event_loop {

template<typename T, size_t Capacity>
class FixedVector {
public:
    bool push_back(const T& value) {
        if (size_ >= Capacity) {
            return false;
        }
        values_[size_++] = value;
        return true;
    }

    void clear() { size_ = 0; }
    size_t size() const { return size_; }
    size_t capacity() const { return Capacity; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ >= Capacity; }

    T& operator[](size_t index) { return values_[index]; }
    const T& operator[](size_t index) const { return values_[index]; }

    T* begin() { return values_; }
    T* end() { return values_ + size_; }
    const T* begin() const { return values_; }
    const T* end() const { return values_ + size_; }

private:
    T values_[Capacity]{};
    size_t size_ = 0;
};

} // namespace pypilot_event_loop
