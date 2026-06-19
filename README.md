# pypilot-event-loop

Portable event-loop and scheduling abstraction for the modular C++ pypilot port.

The module provides a Linux backend using libevent and an Arduino cooperative loop backend. The portable core hides platform-specific event-loop mechanics from the rest of the pypilot-cpp modules.

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
- Linux libevent scheduler
- Arduino cooperative scheduler

The normal pypilot modules should not include libevent, POSIX socket, or Arduino headers.

## Linux backend

Linux uses libevent only. There is no raw `poll()` fallback in this module.

## Arduino backend

Arduino uses a cooperative scheduler intended to be called from `loop()` through `ArduinoLoop::tick()`.

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
