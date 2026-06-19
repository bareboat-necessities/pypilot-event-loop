#pragma once

#if defined(__linux__)
#include "pypilot_event_loop_linux/linux_tcp_server.hpp"
#include "pypilot_event_loop_linux/linux_tcp_client.hpp"
#endif

#if defined(ARDUINO) && defined(PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP)
#include "pypilot_event_loop_arduino/arduino_wifi_tcp.hpp"
#endif

namespace pypilot_event_loop {

#if defined(__linux__)
using NativeTcpServer = LinuxTcpServer;
using NativeTcpConnection = LinuxTcpConnection;
using NativeTcpClient = LinuxTcpClient;
#endif

#if defined(ARDUINO) && defined(PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP)
using NativeTcpServer = ArduinoWiFiTcpServer;
using NativeTcpConnection = ArduinoWiFiTcpConnection;
using NativeTcpClient = ArduinoWiFiTcpClient;
#endif

} // namespace pypilot_event_loop
