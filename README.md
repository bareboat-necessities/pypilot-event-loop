# pypilot-event-loop

Portable event-loop and scheduling abstraction for the modular C++ pypilot port.

The module provides a Linux backend using libevent and an Arduino cooperative backend. Normal application code uses the same `EventLoop`, stream, protocol-reader, pin-event, pin-I/O, UDP, and TCP interfaces where the platform supports them.

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
- event handles for enable/disable/remove with slot reclamation
- digital and analog pin-I/O interfaces
- Linux file-backed digital/analog pin-I/O for GPIO/sysfs/IIO/PWM-style paths
- optional Linux libgpiod digital pin-I/O for claimed GPIO lines
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
event_loop.remove(repeat); // unschedules and reclaims the callback slot

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

`LinuxTcpClient::connect()` starts a nonblocking socket connect and reports completion through `ITcpClientHandler::on_connect()`. Hostname lookup uses `getaddrinfo`, so DNS resolution itself can still block briefly.

## Linux FIFO streams

`LinuxFifoByteStream` wraps a named FIFO as an `IByteStream`. It is a Linux-specific backend class, so it should be used by Linux runtime code or backend tests, not by portable protocol examples.

By default, the FIFO is created if missing and opened `O_RDWR | O_NONBLOCK`. This keeps the fd stable when external writers disconnect, which matters because libevent registrations are tied to a specific fd. Use separate input/output FIFOs for bidirectional IPC if the application must avoid reading its own writes.

## Pin I/O API

Portable code uses `IDigitalInputPin`, `IDigitalOutputPin`, `IAnalogInputPin`, and `IAnalogOutputPin` or the native aliases from `pypilot_event_loop.hpp`.

Linux defaults to file-backed pin I/O:

- `NativeDigitalInputPin(path)` reads integer text from a value file.
- `NativeDigitalOutputPin(path)` writes `0` or `1`.
- `NativeAnalogInputPin(path, max_value)` reads integer text from a raw value file.
- `NativeAnalogOutputPin(path, max_value)` writes a clamped integer value.

These file-backed paths are suitable for prepared sysfs GPIO value files, IIO raw ADC files, PWM duty/value files, test files, and OpenWRT-style lightweight integrations.

For real claimed Linux GPIO lines, include the gpiod backend and opt in before including the umbrella header:

```cpp
#define PYPILOT_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO
#include <pypilot_event_loop.hpp>

pypilot_event_loop::NativeDigitalInputPin input("gpiochip0", 17);
pypilot_event_loop::NativeDigitalOutputPin output("gpiochip0", 27, false);
```

The gpiod backend claims and configures digital lines through libgpiod. Analog input/output remains file-backed because Linux analog/PWM access is normally exposed through IIO/PWM/sysfs-style files or board-specific drivers.

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
examples/PinIoExample/PinIoExample.ino
examples/IntegratedEventLoopExample/IntegratedEventLoopExample.ino
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

Linux FIFO IPC example:

```text
examples/LinuxFifoIpcExample/LinuxFifoIpcExample.cpp
```

The UDP examples demonstrate periodic broadcast send and event-loop-driven datagram receive. The TCP examples demonstrate connect, listen, accept, readable data, line echo, peer disconnect detection, listener shutdown, and active client shutdown.

## Integrated event-loop example

The integrated example handles serial input/output, TCP server, TCP client, UDP broadcast, digital input/output, and analog input/output from one event loop.

Linux run example:

```bash
./build/integrated_event_loop_example \
  /dev/ttyUSB0 /dev/ttyUSB1 \
  /tmp/pypilot-din /tmp/pypilot-dout \
  /tmp/pypilot-ain /tmp/pypilot-aout \
  localhost 20220 20220
```

Arduino ESP32-S3 defaults are compile-safe but board pins can be overridden:

```cpp
#define PYPILOT_SERIAL_OUT Serial1
#define PYPILOT_SERIAL_OUT_RX_PIN 18
#define PYPILOT_SERIAL_OUT_TX_PIN 17
#define PYPILOT_DIGITAL_IN_PIN 4
#define PYPILOT_DIGITAL_OUT_PIN 5
#define PYPILOT_ANALOG_IN_PIN 1
#define PYPILOT_ANALOG_OUT_PIN 2
```

## Build on Linux

```bash
sudo apt-get install -y libevent-dev pkg-config
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`libgpiod-dev` is optional. Install it when building code that defines `PYPILOT_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO` or uses gpiod pin-event sources.

## Arduino compile smoke tests

The CI target is ESP32-S3:

```bash
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/EventLoopTimerExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/LineProtocolExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/FixedHeaderProtocolExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/SerialLineProtocolExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/PinEventExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/PinIoExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/TcpLineClientExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/TcpLineServerExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/UdpBroadcastSenderExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/UdpReceiverExample
arduino-cli compile --fqbn esp32:esp32:esp32s3 --libraries . examples/IntegratedEventLoopExample
```

WiFi UDP/TCP examples require a WiFi-capable Arduino core and compile-time WiFi credentials/host macros.

## OpenWRT note

OpenWRT builds should provide libevent in the target SDK/toolchain. Named FIFO IPC uses normal Linux file descriptors, so it should work on OpenWRT when the selected filesystem supports FIFOs and the process has permissions for the FIFO path. libgpiod is optional and only needed for code that opts into claimed GPIO lines.
