#pragma once

#if defined(ARDUINO)
#define PYPILOT_EVENT_LOOP_EXAMPLE_MAIN(setup_fn, loop_fn) \
    void setup() { setup_fn(); } \
    void loop() { loop_fn(); }
#else
#define PYPILOT_EVENT_LOOP_EXAMPLE_MAIN(setup_fn, loop_fn) \
    int main() { \
        setup_fn(); \
        for (int pypilot_event_loop_example_i = 0; pypilot_event_loop_example_i < 1000; ++pypilot_event_loop_example_i) { \
            loop_fn(); \
        } \
        return 0; \
    }
#endif
