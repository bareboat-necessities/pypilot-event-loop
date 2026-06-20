#pragma once

#if defined(ARDUINO)
#include "pypilot_event_loop_arduino/arduino_pin_io.hpp"
#endif

#if defined(__linux__)
#include "pypilot_event_loop_linux/linux_file_pin_io.hpp"
#if defined(PYPILOT_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO)
#include "pypilot_event_loop_linux/linux_gpiod_pin_io.hpp"
#endif
#endif

namespace pypilot_event_loop {

#if defined(ARDUINO)
using NativeDigitalInputPin = ArduinoDigitalInputPin;
using NativeDigitalOutputPin = ArduinoDigitalOutputPin;
using NativeAnalogInputPin = ArduinoAnalogInputPin;
using NativeAnalogOutputPin = ArduinoAnalogOutputPin;
#endif

#if defined(__linux__)
#if defined(PYPILOT_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO)
using NativeDigitalInputPin = LinuxGpiodDigitalInputPin;
using NativeDigitalOutputPin = LinuxGpiodDigitalOutputPin;
#else
using NativeDigitalInputPin = LinuxFileDigitalInputPin;
using NativeDigitalOutputPin = LinuxFileDigitalOutputPin;
#endif
using NativeAnalogInputPin = LinuxFileAnalogInputPin;
using NativeAnalogOutputPin = LinuxFileAnalogOutputPin;
#endif

} // namespace pypilot_event_loop
