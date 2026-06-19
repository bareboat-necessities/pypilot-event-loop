# pypilot-event-loop

Portable event-loop and scheduling abstraction for the modular C++ pypilot port.

The module provides a Linux backend using libevent and an Arduino cooperative loop backend. Normal application code uses the same public `NativeClock` and `NativeScheduler` aliases on both platforms; the aliases select the platform backend at compile time.

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
- test scheduler
- native platform clock alias
- native platform scheduler alias
- Linux libevent scheduler
- Arduino cooperative scheduler

The normal pypilot modules should not include libevent, POSIX socket, or Arduino headers.

## Common API

Use this style in normal code:

```cpp
#include <pypilot_event_loop.hpp>

pypilot_event_loop::NativeClock clock;
pypilot_event_loop::NativeScheduler scheduler(clock);
```

On Linux this resolves to the libevent backend. On Arduino this resolves to the cooperative Arduino backend.

## Linux backend

Linux uses libevent only. There is no raw `poll()` fallback in this module.

## Arduino backend

Arduino uses the same `NativeClock` and `NativeScheduler` names. Call `scheduler.run_once()` from the sketch `loop()` function.

## Build on Linux

```bash
sudo apt-get install -y libevent-dev pkg-config
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Arduino compile smoke test

```bash
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/EventLoopSmoke
```

## OpenWRT note

OpenWRT builds should provide libevent in the target SDK/toolchain. If libevent is missing, the Linux backend intentionally fails at configure time.
