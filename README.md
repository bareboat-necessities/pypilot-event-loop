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
- byte/data-ready callbacks
- line-delimited protocol readers
- fixed-size frame protocol readers
- header+payload protocol readers
- native file descriptor integration for Linux streams
- Linux named FIFO byte streams
- event handles for enable/disable/remove
- static byte streams for portable tests/examples
- static datagram streams for portable tests/examples
- sampled digital pin input abstraction
- fd-backed pin event source abstraction
- Linux libgpiod pin event source
- legacy Linux sysfs pin event fallback
- Arduino cooperative pin event source
- Arduino interrupt guard
- Arduino interrupt-backed pin event source
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

pypilot_event_loop::EventHandle repeat = event_loop.on_repeat(1000, []() {
    // runs once per second
});

event_loop.disable(repeat);
event_loop.enable(repeat);
event_loop.remove(repeat);

event_loop.on_delay(500, []() {
    // runs once after 500 ms
});
```

On Linux, `EventLoop` uses the libevent backend. On Arduino, it uses the cooperative Arduino backend.

## Byte/data-ready API

Use `on_bytes_ready()` for raw byte and datagram input. Do not poll streams by timer in application code.

```cpp
pypilot_event_loop::EventLoop<> event_loop;

pypilot_event_loop::EventHandle input = event_loop.on_bytes_ready(stream, [&]() {
    uint8_t buf[128];
    const int n = stream.read(buf, sizeof(buf));
    if (n > 0) {
        // process bytes
    }
});
```

On Linux, fd-backed streams return `native_fd() >= 0` and are registered with libevent fd readiness internally. On Arduino and static streams, `native_fd() == -1`, so the same callback is checked cooperatively from `tick()`.

## Protocol readers

Use protocol readers when the application should receive complete messages rather than raw readable bytes.

Line-delimited protocol:

```cpp
pypilot_event_loop::LineProtocolReader<128> lines(stream, {}, [](pypilot_event_loop::LineView line) {
    // complete line, with optional CR stripping
});

event_loop.on_bytes_ready(stream, [&]() {
    lines.poll(event_loop.clock().micros());
});
```

Fixed-size frame protocol:

```cpp
pypilot_event_loop::FixedFrameProtocolReader<64> frames(
    stream,
    pypilot_event_loop::FixedFrameProtocolOptions{32},
    [](pypilot_event_loop::FrameView frame) {
        // exactly 32 bytes
    });
```

Header+payload protocol:

```cpp
pypilot_event_loop::HeaderPayloadProtocolReader<512> framed(
    stream,
    pypilot_event_loop::HeaderPayloadProtocolOptions{4, 256, payload_size_from_header},
    [](pypilot_event_loop::FrameView frame) {
        // complete header + payload
    });
```

## Linux FIFO streams

`LinuxFifoByteStream` wraps a named FIFO as an `IByteStream`.

```cpp
pypilot_event_loop::LinuxFifoByteStream fifo("/tmp/pypilot-events.fifo");

pypilot_event_loop::LineProtocolReader<256> lines(fifo, {}, [](pypilot_event_loop::LineView line) {
    // complete FIFO line
});

event_loop.on_bytes_ready(fifo, [&]() {
    lines.poll(event_loop.clock().micros());
});
```

By default, the FIFO is created if missing and opened `O_RDWR | O_NONBLOCK`. This keeps the fd stable when external writers disconnect, which matters because libevent registrations are tied to a specific fd. Use separate input/output FIFOs for bidirectional IPC if the application must avoid reading its own writes.

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

Production edge-event sources implement `IPinEventSource` and are registered with `EventLoop::on_pin_event()`:

```cpp
pypilot_event_loop::LinuxGpiodPinEventSource pin(
    "/dev/gpiochip0", 17, pypilot_event_loop::PinEventType::Change);

event_loop.on_pin_event(pin, [](const pypilot_event_loop::PinEvent& event) {
    // event.source_id, event.sequence, event.timestamp_us, event.level
});
```

Linux gpiod sources expose a native fd, so libevent wakes the callback only when the GPIO request fd is readable. Arduino and other cooperative sources return `native_fd() == -1`, so the same callback is checked from `tick()`.

Sampled pins still use `IPinInput`, `PinEventTask`, and `LambdaPinEventTask` for simpler polling/test cases.

## Arduino interrupt-backed pin events

Use `ArduinoInterruptPinEventSource` when a hardware ISR should wake normal loop-context handling. The ISR only records a pending event; the user callback runs later from `event_loop.tick()`.

```cpp
pypilot_event_loop::ArduinoInterruptPinEventSource button_event(
    2,
    pypilot_event_loop::PinEventType::RisingEdge);

void button_isr() {
    button_event.notify_from_isr(digitalRead(2) == HIGH);
}

void setup() {
    attachInterrupt(digitalPinToInterrupt(2), button_isr, RISING);
    event_loop.on_pin_event(button_event, [](const pypilot_event_loop::PinEvent& event) {
        // normal loop context, not ISR context
    });
}
```

Available platform sources:

```text
LinuxGpiodPinEventSource
LinuxSysfsPinEventSource
ArduinoDigitalPinEventSource
ArduinoInterruptPinEventSource
```

## Lower-level common API

The lower-level API is also platform-neutral:

```cpp
#include <pypilot_event_loop.hpp>

pypilot_event_loop::NativeClock clock;
pypilot_event_loop::NativeScheduler scheduler(clock);
```

## Byte stream examples

The Linux and Arduino byte stream examples use the same public data-ready API: `EventLoop<>` and `on_bytes_ready()`.

```text
examples/linux/byte_stream_pipe.cpp
examples/arduino/SerialByteStreamEcho/SerialByteStreamEcho.ino
```

## Datagram stream examples

The Linux and Arduino datagram examples use the same public data-ready API: `EventLoop<>` and `on_bytes_ready()`.

```text
examples/linux/datagram_socketpair.cpp
examples/arduino/DatagramStreamExample/DatagramStreamExample.ino
```

## Protocol examples

```text
examples/linux/line_protocol_pipe.cpp
examples/linux/fixed_frame_pipe.cpp
examples/linux/fifo_line_reader.cpp
```

## Pin event examples

```text
examples/linux/gpiod_pin_event.cpp
examples/arduino/PinEventExample/PinEventExample.ino
examples/arduino/InterruptPinEventExample/InterruptPinEventExample.ino
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
sudo apt-get install -y libevent-dev libgpiod-dev pkg-config
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Arduino compile smoke tests

```bash
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/EventLoopSmoke
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/SerialByteStreamEcho
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/DatagramStreamExample
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/PinEventExample
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/arduino/InterruptPinEventExample
```

## OpenWRT note

OpenWRT builds should provide libevent and libgpiod in the target SDK/toolchain. If either is missing, the Linux backend intentionally fails at configure time. Named FIFO IPC uses normal Linux file descriptors, so it should work on OpenWRT when the selected filesystem supports FIFOs and the process has permissions for the FIFO path.
