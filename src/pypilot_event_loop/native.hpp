#pragma once

#if defined(ARDUINO)
#include "pypilot_event_loop_arduino/arduino_clock.hpp"
#include "pypilot_event_loop_arduino/arduino_loop.hpp"

namespace pypilot_event_loop {
using NativeClock = ArduinoClock;
using NativeScheduler = ArduinoLoop;
} // namespace pypilot_event_loop

#elif defined(__linux__)
#include "pypilot_event_loop_linux/monotonic_clock.hpp"
#include "pypilot_event_loop_linux/libevent_loop.hpp"

namespace pypilot_event_loop {
using NativeClock = LinuxMonotonicClock;
using NativeScheduler = LinuxLibeventLoop;
} // namespace pypilot_event_loop

#endif
