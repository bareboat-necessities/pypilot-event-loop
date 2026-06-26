#pragma once

#include <stdint.h>

namespace async_event_loop {

enum class DigitalPinMode : uint8_t {
    Input,
    InputPullup,
    Output
};

class IDigitalInputPin {
public:
    virtual ~IDigitalInputPin() = default;
    virtual bool valid() const = 0;
    virtual bool read() = 0;
};

class IDigitalOutputPin {
public:
    virtual ~IDigitalOutputPin() = default;
    virtual bool valid() const = 0;
    virtual bool write(bool high) = 0;
};

class IAnalogInputPin {
public:
    virtual ~IAnalogInputPin() = default;
    virtual bool valid() const = 0;
    virtual int read() = 0;
    virtual int max_value() const = 0;
};

class IAnalogOutputPin {
public:
    virtual ~IAnalogOutputPin() = default;
    virtual bool valid() const = 0;
    virtual bool write(int value) = 0;
    virtual int max_value() const = 0;
};

} // namespace async_event_loop
