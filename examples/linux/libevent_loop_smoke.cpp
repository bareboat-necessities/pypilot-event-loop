#include <iostream>

#include "pypilot_event_loop.hpp"

int main() {
    pypilot_event_loop::EventLoop<> event_loop;
    if (!event_loop.valid()) {
        return 1;
    }

    int count = 0;
    if (!event_loop.onRepeat(100, [&event_loop, &count]() {
            std::cout << "tick " << count << std::endl;
            if (++count >= 3) {
                event_loop.request_exit();
            }
        })) {
        return 2;
    }

    event_loop.run_forever();
    return 0;
}
