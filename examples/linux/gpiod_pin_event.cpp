#include <cstdlib>
#include <iostream>

#include "pypilot_event_loop.hpp"
#include "pypilot_event_loop_linux/linux_gpiod_pin_event_source.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "usage: " << argv[0] << " /dev/gpiochipX LINE_OFFSET" << std::endl;
        return 0;
    }

    const char* chip_path = argv[1];
    const unsigned int line_offset = static_cast<unsigned int>(std::strtoul(argv[2], nullptr, 10));

    pypilot_event_loop::EventLoop<> event_loop;
    pypilot_event_loop::LinuxGpiodPinEventSource pin(
        chip_path,
        line_offset,
        pypilot_event_loop::PinEventType::Change);

    if (!event_loop.valid() || !pin.valid()) {
        return 1;
    }

    event_loop.on_pin_event(pin, [](const pypilot_event_loop::PinEvent& event) {
        std::cout << "pin " << event.source_id
                  << " seq " << event.sequence
                  << " time_us " << event.timestamp_us
                  << " level " << event.level
                  << std::endl;
    });

    event_loop.run_forever();
    return 0;
}
