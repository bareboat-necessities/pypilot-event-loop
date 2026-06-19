#pragma once

#if defined(__linux__)
#include "pypilot_event_loop_linux/linux_tcp_server.hpp"
#endif

namespace pypilot_event_loop {

#if defined(__linux__)
using NativeTcpServer = LinuxTcpServer;
using NativeTcpConnection = LinuxTcpConnection;
#endif

} // namespace pypilot_event_loop
