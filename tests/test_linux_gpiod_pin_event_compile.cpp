#define ASYNC_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO
#include "async_event_loop_linux/linux_gpiod_pin_event_source.hpp"

#include <cassert>

int main() {
    async_event_loop::LinuxGpiodPinEventSource source(
        "__async_event_loop_missing_gpiochip__",
        0,
        async_event_loop::PinEventType::Change);

    assert(!source.valid());
    assert(source.native_fd() < 0);
    return 0;
}
