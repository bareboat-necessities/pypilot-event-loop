# pypilot-event-loop

Portable event-loop and scheduling abstraction for the modular C++ pypilot port.

Phase 1 provides the Linux backend using libevent. Arduino support is intentionally left for a later phase.

## Purpose

`pypilot-event-loop` hides platform-specific event-loop mechanics from the rest of the pypilot-cpp modules.

The core APIs expose:

- monotonic clocks
- runtime tasks
- periodic timers
- one-shot timers
- byte streams
- datagram streams
- fixed-size event queues
- fake/test loop
- Linux libevent loop

The normal pypilot modules should not include libevent, POSIX socket, or Arduino headers.

## Linux backend

Linux uses libevent only. There is no raw `poll()` fallback in this module.

## Build

```bash
sudo apt-get install -y libevent-dev pkg-config
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## OpenWRT note

OpenWRT builds should provide libevent in the target SDK/toolchain. If libevent is missing, the Linux backend intentionally fails at configure time.

## Later phase

Arduino support will add:

- `ArduinoClock`
- `ArduinoLoop`
- `ArduinoSerialStream`
- Arduino compile CI
