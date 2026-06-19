# pypilot-event-loop

Portable event-loop and scheduling abstraction for the modular C++ pypilot port.

The module provides a Linux backend using libevent and an Arduino cooperative backend. Normal application code can use one `EventLoop` object on both platforms, while lower-level code can still use `IRuntimeTask`, `IScheduler`, `NativeClock`, and `NativeScheduler` directly.

## Purpose

The core APIs expose:

- monotonic clocks
- runtime tasks
- scheduler abstraction
- periodic timers
- one-shot timers
- byte streams
- datagram streams
- fixed-size event queues
- fixed-storage callback tasks
- ReactESP-style `EventLoop` callback facade
- native platform clock alias
- native platform scheduler alias
- Linux libevent scheduler
- Arduino cooperative scheduler

The normal pypilot modules should not include libevent, POSIX socket, or Arduino headers.

## Common callback API

Use this style in normal code:

```cpp
#include <pypilot_event_loop.hpp>

pypilot_event_loop::EventLoop<> event_loop;

event_loop.onRepeat(1000, []() {
    // runs once per second
});

event_loop.onDelay(500, []() {
    // runs once after 500 ms
});
```

On Linux, `EventLoop` uses the libevent backend. On Arduino, it uses the cooperative Arduino backend.

## Lower-level common API

The lower-level API is also platform-neutral:

```cpp
#include <pypilot_event_loop.hpp>

pypilot_event_loop::NativeClock clock;
pypilot_event_loop::NativeScheduler scheduler(clock);
```

## Byte stream example

Linux pipe-backed byte stream usage is in:

```text
examples/linux/byte_stream_pipe.cpp
```

Arduino Serial byte stream usage is in:

```text
examples/arduino/SerialByteStreamEcho/SerialByteStreamEcho.ino
```

## Datagram stream example

Linux datagram stream usage is in:

```text
examples/linux/datagram_socketpair.cpp
```

It uses a local `socketpair(AF_UNIX, SOCK_DGRAM, ...)` so the example does not need a network interface.

## Linux backend

Linux uses libevent only. There is no raw `poll()` fallback in this module.

## Arduino backend

Arduino can call the same facade from the sketch loop:

```cpp
void loop() {
    event_loop.tick();
}
```

## Build on Linux

```bash
sudo apt-get install -y libevent-dev pkg-config
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Arduino compile smoke tests

```bash
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/EventLoopSmoke
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/SerialByteStreamEcho
```

## OpenWRT note

OpenWRT builds should provide libevent in the target SDK/toolchain. If libevent is missing, the Linux backend intentionally fails at configure time.
