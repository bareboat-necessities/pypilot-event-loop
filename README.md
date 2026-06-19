# pypilot-event-loop

Portable event-loop and scheduling abstraction for the modular C++ pypilot port.

The module provides a Linux backend using libevent and an Arduino cooperative backend. Normal application code uses the same `EventLoop`, stream, protocol-reader, pin-event, UDP, and TCP interfaces where the platform supports them.

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
- UDP datagram interfaces with broadcast sender and receiver examples
- TCP listener/connection interfaces
- TCP client connection interfaces
- Linux UDP/TCP backends using libevent fd readiness and native sockets
- Arduino UDP/TCP backends using WiFi only, gated by `PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP` or `PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP`
- Linux named FIFO byte streams for runtime/backend code
- native serial stream aliases for Arduino `Serial` and Linux tty devices
- event handles for enable/disable/remove
- `IPinEventSource`
- Linux gpiod pin events for runtime/backend code
- Arduino cooperative and interrupt-backed pin events for runtime/backend code
- fixed-storage callback tasks
- native platform clock/scheduler aliases
- native console stream alias

The normal pypilot modules should not include libevent, POSIX socket, fd, test-support, example-support, or Arduino backend headers.

## Portable example style

Portable examples use one optional Arduino include block at the top, then include only the umbrella header:

```cpp
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <pypilot_event_loop.hpp>
```

Small throwaway streams or sources used only to drive an example stay local to that example file. Shared fixtures belong in `tests/support`, not in public headers and not in `src`.

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

## Serial line protocol example

`examples/SerialLineProtocolExample/SerialLineProtocolExample.ino` is a unified Arduino/Linux example. On Arduino it wraps `Serial`. On Linux it opens a tty path with `NativeSerialStream` and uses libevent fd readiness.

Linux run example:

```bash
./build/serial_line_protocol_example /dev/ttyUSB0 115200
```

Send newline-delimited messages. The example echoes each received line as `line: ...`.

## UDP datagram API

Linux exposes `NativeUdpDatagramStream` using IPv4 UDP sockets. Arduino exposes the same alias only when `PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP` is defined before including `pypilot_event_loop.hpp`. The Arduino UDP backend is WiFi-only and uses `WiFiUDP`.

```cpp
#define PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_UDP
#include <pypilot_event_loop.hpp>
```

Linux broadcast sender example:

```bash
./build/udp_broadcast_sender_example 255.255.255.255 20225
```

Linux receiver example:

```bash
./build/udp_receiver_example 20225
```

## TCP server and client API

Linux exposes `NativeTcpServer` and `NativeTcpClient`. Arduino exposes the same aliases only when `PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP` is defined before including `pypilot_event_loop.hpp`. The Arduino TCP backend is WiFi-only and uses `WiFiServer`/`WiFiClient`; it does not pull in Ethernet or non-WiFi transports.

```cpp
#define PYPILOT_EVENT_LOOP_ENABLE_ARDUINO_WIFI_TCP
#include <pypilot_event_loop.hpp>
```

Server handlers implement `ITcpServerHandler`; client handlers implement `ITcpClientHandler`. Both use `ITcpConnection` for `read_line()`, `read_exact()`, `write()`, and close handling.

The listener supports reusable bind by default through `TcpListenOptions::reuse_address`. Use `NativeTcpServer::close()` to shut down the listener and close active accepted sockets.

## Linux FIFO streams

`LinuxFifoByteStream` wraps a named FIFO as an `IByteStream`. It is a Linux-specific backend class, so it should be used by Linux runtime code or backend tests, not by portable protocol examples.

By default, the FIFO is created if missing and opened `O_RDWR | O_NONBLOCK`. This keeps the fd stable when external writers disconnect, which matters because libevent registrations are tied to a specific fd. Use separate input/output FIFOs for bidirectional IPC if the application must avoid reading its own writes.

## Pin event API

Production edge/state sources implement `IPinEventSource` and are registered with `EventLoop::on_pin_event()`.

Linux runtime code can use `LinuxGpiodPinEventSource`. Arduino runtime code can use `ArduinoDigitalPinEventSource` or `ArduinoInterruptPinEventSource`. Portable examples define their own local example source instead of importing backend or test helpers.

## Examples

Shared Linux/Arduino examples:

```text
examples/EventLoopTimerExample/EventLoopTimerExample.ino
examples/LineProtocolExample/LineProtocolExample.ino
examples/FixedHeaderProtocolExample/FixedHeaderProtocolExample.ino
examples/SerialLineProtocolExample/SerialLineProtocolExample.ino
examples/PinEventExample/PinEventExample.ino
```

Linux/Arduino-WiFi UDP examples:

```text
examples/UdpBroadcastSenderExample/UdpBroadcastSenderExample.ino
examples/UdpReceiverExample/UdpReceiverExample.ino
```

Linux/Arduino-WiFi TCP examples:

```text
examples/TcpLineClientExample/TcpLineClientExample.ino
examples/TcpLineServerExample/TcpLineServerExample.cpp
examples/TcpLineServerExample/TcpLineServerExample.ino
```

Linux TCP server examples:

```text
examples/TcpFixedHeaderServerExample/TcpFixedHeaderServerExample.cpp
examples/TcpDisconnectServerExample/TcpDisconnectServerExample.cpp
```

The UDP examples demonstrate periodic broadcast send and event-loop-driven datagram receive. The TCP examples demonstrate connect, listen, accept, readable data, line echo, peer disconnect detection, listener shutdown, and active client shutdown.

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
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/SerialLineProtocolExample
arduino-cli compile --fqbn arduino:avr:mega --libraries . examples/PinEventExample
```

WiFi UDP/TCP examples require a WiFi-capable Arduino core such as ESP32 and compile-time WiFi credentials/host macros.

## OpenWRT note

OpenWRT builds should provide libevent and libgpiod in the target SDK/toolchain. If either is missing, the Linux backend intentionally fails at configure time. Named FIFO IPC uses normal Linux file descriptors, so it should work on OpenWRT when the selected filesystem supports FIFOs and the process has permissions for the FIFO path.
