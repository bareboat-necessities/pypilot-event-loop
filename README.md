# pypilot-event-loop

Portable event-loop and scheduling abstraction for the modular C++ pypilot port.

The module provides a Linux backend using libevent and an Arduino cooperative backend. Normal application code uses the same `EventLoop`, stream, protocol-reader, and pin-event APIs on both platforms.

## Public model

The public API exposes:

- monotonic clocks
- runtime tasks
- periodic and one-shot timers
- byte streams and datagram streams
- byte/data-ready callbacks
- line-delimited protocol readers
- fixed-size frame protocol readers
- header+payload protocol readers
- Linux named FIFO byte streams
- event handles for enable/disable/remove
- static byte streams and static pin-event sources for portable tests/examples
- `IPinEventSource`
- Linux gpiod pin events
- Arduino cooperative and interrupt-backed pin events
- fixed-storage callback tasks
- native platform clock/scheduler aliases
- native console stream alias

The normal pypilot modules should not include libevent, POSIX socket, fd, or Arduino backend headers.

## Portable example style

Portable examples use one optional Arduino include block at the top, then include only the umbrella header:

```cpp
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <pypilot_event_loop.hpp>
```

The rest of the example should use only portable names such as `EventLoop`, `StaticByteStream`, `LineProtocolReader`, and `StaticPinEventSource`.

## Common callback API

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

## Byte/data-ready API

Use `on_bytes_ready()` for raw byte and datagram input. Protocol examples should usually feed a protocol reader rather than manually parse bytes.

```cpp
pypilot_event_loop::EventHandle input = event_loop.on_bytes_ready(stream, [&]() {
    uint8_t buf[128];
    const int n = stream.read(buf, sizeof(buf));
    if (n > 0) {
        // process bytes
    }
});
```

## Protocol readers

Line-delimited protocol:

```cpp
pypilot_event_loop::LineProtocolReader<128> lines(stream, {}, [](pypilot_event_loop::LineView line) {
    // complete line, with optional CR stripping
});

event_loop.on_bytes_ready(stream, [&]() {
    lines.poll(event_loop.clock().micros());
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

`LinuxFifoByteStream` wraps a named FIFO as an `IByteStream`. It is a Linux-specific backend class, so it should be used by Linux runtime code or backend tests, not by portable protocol examples.

By default, the FIFO is created if missing and opened `O_RDWR | O_NONBLOCK`. This keeps the fd stable when external writers disconnect, which matters because libevent registrations are tied to a specific fd. Use separate input/output FIFOs for bidirectional IPC if the application must avoid reading its own writes.

## Pin event API

Production edge/state sources implement `IPinEventSource` and are registered with `EventLoop::on_pin_event()`.

```cpp
pypilot_event_loop::StaticPinEventSource<4> source;

event_loop.on_pin_event(source, [](const pypilot_event_loop::PinEvent& event) {
    // portable pin event callback
});
```

Linux runtime code can use `LinuxGpiodPinEventSource`. Arduino runtime code can use `ArduinoDigitalPinEventSource` or `ArduinoInterruptPinEventSource`. Portable examples use `StaticPinEventSource`.

## Examples

The public examples are shared between Linux and Arduino builds:

```text
examples/EventLoopTimerExample/EventLoopTimerExample.ino
examples/LineProtocolExample/LineProtocolExample.ino
examples/FixedHeaderProtocolExample/FixedHeaderProtocolExample.ino
examples/PinEventExample/PinEventExample.ino
```

These examples intentionally do not include Linux or Arduino backend headers and do not name backend-specific classes.

## Build on Linux

```bash
sudo apt-get install -y libevent-dev libgpiod-dev pkg-config
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Arduino compile smoke tests

```bash
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/EventLoopTimerExample
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/LineProtocolExample
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/FixedHeaderProtocolExample
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/PinEventExample
```

## OpenWRT note

OpenWRT builds should provide libevent and libgpiod in the target SDK/toolchain. If either is missing, the Linux backend intentionally fails at configure time. Named FIFO IPC uses normal Linux file descriptors, so it should work on OpenWRT when the selected filesystem supports FIFOs and the process has permissions for the FIFO path.
