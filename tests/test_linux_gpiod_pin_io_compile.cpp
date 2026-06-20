#define PYPILOT_EVENT_LOOP_ENABLE_LINUX_GPIOD_PIN_IO
#include "pypilot_event_loop.hpp"

#include <cassert>

int main() {
    pypilot_event_loop::LinuxGpiodDigitalInputPin input("__pypilot_event_loop_missing_gpiochip__", 0);
    pypilot_event_loop::LinuxGpiodDigitalOutputPin output("__pypilot_event_loop_missing_gpiochip__", 0, false);

    assert(!input.valid());
    assert(!output.valid());

    pypilot_event_loop::NativeDigitalInputPin native_input("__pypilot_event_loop_missing_gpiochip__", 0);
    pypilot_event_loop::NativeDigitalOutputPin native_output("__pypilot_event_loop_missing_gpiochip__", 0, false);

    assert(!native_input.valid());
    assert(!native_output.valid());
    return 0;
}
