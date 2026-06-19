#pragma once

#if defined(__linux__)
#include "pypilot_event_loop_linux/linux_udp_datagram_stream.hpp"
#endif

#if defined(ARDUINO) && defined(PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP)
#include "pypilot_event_loop_arduino/arduino_wifi_udp.hpp"
#endif

namespace pypilot_event_loop {

#if defined(__linux__)
using NativeUdpDatagramStream = LinuxUdpDatagramStream;
#endif

#if defined(ARDUINO) && defined(PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP)
using NativeUdpDatagramStream = ArduinoWiFiUdpDatagramStream;
#endif

} // namespace pypilot_event_loop
