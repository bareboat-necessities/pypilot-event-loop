#pragma once

#include "async_event_loop/event_loop.hpp"
#include "async_event_loop/udp.hpp"
#include "async_event_loop/protocol_reader.hpp"
#include "async_event_loop/tcp.hpp"
#include "async_event_loop/tcp_line_protocol.hpp"
#include "async_event_loop/pin_io.hpp"
#include "async_event_loop/scheduler.hpp"
#include "async_event_loop/native.hpp"
#include "async_event_loop/native_stream.hpp"
#include "async_event_loop/native_udp.hpp"
#include "async_event_loop/native_tcp.hpp"
#include "async_event_loop/native_pin_io.hpp"

#if defined(ARDUINO)
#include "async_event_loop_arduino/arduino_wifi.hpp"
#endif
