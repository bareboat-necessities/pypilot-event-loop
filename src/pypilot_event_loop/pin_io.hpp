#pragma once

#include <stdint.h>

namespace pypilot_event_loop {

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
    virtual bool read() = 0;
    virtual void write(bool high) = 0;
};

class IAnalogInputPin {
public:
    virtual ~IAnalogInputPin() = default;
    virtual bool valid() const = 0;
    virtual int read_raw() = 0;
};

class IAnalogOutputPin {
public:
    virtual ~IAnalogOutputPin() = default;
    virtual bool valid() const = 0;
    virtual int read_raw() = 0;
    virtual void write_raw(int value) = 0;
};

} // namespace pypilot_event_loop
