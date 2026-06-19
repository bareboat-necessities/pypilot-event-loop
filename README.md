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
- stream readiness callbacks
- native file descriptor integration for Linux streams
- static byte streams for portable tests/examples
- static datagram streams for portable tests/examples
- digital pin input abstraction
- pin event tasks
- lambda-backed pin event tasks
- bounded event queues with overflow policies
- SPSC event queues for interrupt/single-producer handoff
- fixed-storage callback tasks
- `EventLoop` callback facade
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

event_loop.on_repeat(1000, []() {
    // runs once per second
});

event_loop.on_delay(500, []() {
    // runs once after 500 ms
});
```

On Linux, `EventLoop` uses the libevent backend. On Arduino, it uses the cooperative Arduino backend.

## Stream readiness API

Use `on_readable()` for byte and datagram input. Do not poll streams by timer in application code.

```cpp
pypilot_event_loop::EventLoop<> event_loop;

event_loop.on_readable(stream, [&]() {
    uint8_t buf[128];
    const int n = stream.read(buf, sizeof(buf));
    if (n > 0) {
        // process bytes
    }
});
```

On Linux, fd-backed streams return `native_fd() >= 0` and are registered with libevent fd readiness. On Arduino and static streams, `native_fd() == -1`, so the same callback is checked cooperatively from `tick()`.

## Event queues

Normal runtime queue:

```cpp
pypilot_event_loop::BoundedEventQueue<32,
    pypilot_event_loop::EventQueueOverflowPolicy::DropOldest> queue;
```

Available overflow policies:

```text
RejectNew
DropOldest
DropNewest
CoalesceBySource
```

The queue assigns sequence numbers, tracks pushed/popped/dropped/coalesced counts, and exposes overflow diagnostics.

Interrupt/single-producer handoff queue:

```cpp
pypilot_event_loop::SpscEventQueue<16> queue;
```

Use this for small POD event handoff from interrupt-like producer context to the normal event loop. Do not use it as a general multi-producer Linux work queue.

## Pin event API

Use `PinEventTask` when a pin event should invoke a named task:

```cpp
pypilot_event_loop::PinEventTask task(pin, target_task,
                                      pypilot_event_loop::PinEventType::RisingEdge,
                                      5000);
```

Use `LambdaPinEventTask` when a pin event should invoke a lambda:

```cpp
pypilot_event_loop::LambdaPinEventTask<> task(pin,
    pypilot_event_loop::PinEventType::FallingEdge,
    []() {
        // pin event callback
    });
```

The pin abstraction is platform-neutral, so Raspberry Pi GPIO and Arduino digital pins can both adapt to `IPinInput`.

## Lower-level common API

The lower-level API is also platform-neutral:

```cpp
#include <pypilot_event_loop.hpp>

pypilot_event_loop::NativeClock clock;
pypilot_event_loop::NativeScheduler scheduler(clock);
```

## Byte stream examples

The Linux and Arduino byte stream examples use the same public readiness API: `EventLoop<>` and `on_readable()`.

```text
examples/linux/byte_stream_pipe.cpp
examples/arduino/SerialByteStreamEcho/SerialByteStreamEcho.ino
```

## Datagram stream examples

The Linux and Arduino datagram examples use the same public readiness API: `EventLoop<>` and `on_readable()`.

```text
examples/linux/datagram_socketpair.cpp
examples/arduino/DatagramStreamExample/DatagramStreamExample.ino
```

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
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/DatagramStreamExample
```

## OpenWRT note

OpenWRT builds should provide libevent in the target SDK/toolchain. If libevent is missing, the Linux backend intentionally fails at configure time.
