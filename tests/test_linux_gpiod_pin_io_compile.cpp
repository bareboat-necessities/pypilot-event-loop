#define ASYNC_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO
#include "async_event_loop.hpp"

#include <cassert>

int main() {
    async_event_loop::LinuxGpiodDigitalInputPin input("__async_event_loop_missing_gpiochip__", 0);
    async_event_loop::LinuxGpiodDigitalOutputPin output("__async_event_loop_missing_gpiochip__", 0, false);

    assert(!input.valid());
    assert(!output.valid());

    async_event_loop::NativeDigitalInputPin native_input("__async_event_loop_missing_gpiochip__", 0);
    async_event_loop::NativeDigitalOutputPin native_output("__async_event_loop_missing_gpiochip__", 0, false);

    assert(!native_input.valid());
    assert(!native_output.valid());
    return 0;
}
