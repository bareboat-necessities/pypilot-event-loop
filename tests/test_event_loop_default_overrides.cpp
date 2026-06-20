#define PYPILOT_EVENT_LOOP_DEFAULT_MAX_CALLBACKS 4
#define PYPILOT_EVENT_LOOP_DEFAULT_CALLBACK_STORAGE_SIZE 32
#include "pypilot_event_loop.hpp"

#include <cassert>

int main() {
    pypilot_event_loop::EventLoop<> loop;
    assert(loop.valid());
    return 0;
}
