#if defined(ARDUINO)
#include <Arduino.h>
#ifndef PYPILOT_DIGITAL_IN_PIN
#define PYPILOT_DIGITAL_IN_PIN 4
#endif
#ifndef PYPILOT_DIGITAL_OUT_PIN
#define PYPILOT_DIGITAL_OUT_PIN 5
#endif
#ifndef PYPILOT_ANALOG_IN_PIN
#define PYPILOT_ANALOG_IN_PIN 1
#endif
#ifndef PYPILOT_ANALOG_OUT_PIN
#define PYPILOT_ANALOG_OUT_PIN 2
#endif
#else
#include <cstdlib>
#include <iostream>
#endif

#include <async_event_loop.hpp>

using namespace async_event_loop;

EventLoop<> event_loop;
bool digital_out_level = false;
int analog_out_value = 0;

IDigitalInputPin* digital_in = nullptr;
IDigitalOutputPin* digital_out = nullptr;
IAnalogInputPin* analog_in = nullptr;
IAnalogOutputPin* analog_out = nullptr;

#if defined(ARDUINO)
bool setup_failed = false;
#endif

static void print_text(const char* text) {
#if defined(ARDUINO)
    Serial.print(text);
#else
    std::cout << text;
#endif
}

static void print_number(int value) {
#if defined(ARDUINO)
    Serial.print(value);
#else
    std::cout << value;
#endif
}

#if defined(ARDUINO)
static bool setup_pins() {
    static NativeDigitalInputPin din(PYPILOT_DIGITAL_IN_PIN, DigitalPinMode::InputPullup);
    static NativeDigitalOutputPin dout(PYPILOT_DIGITAL_OUT_PIN);
    static NativeAnalogInputPin ain(PYPILOT_ANALOG_IN_PIN);
    static NativeAnalogOutputPin aout(PYPILOT_ANALOG_OUT_PIN);
    digital_in = &din;
    digital_out = &dout;
    analog_in = &ain;
    analog_out = &aout;
    return digital_in->valid() && digital_out->valid() && analog_in->valid() && analog_out->valid();
}
#else
static bool setup_pins(const char* digital_in_path,
                       const char* digital_out_path,
                       const char* analog_in_path,
                       const char* analog_out_path) {
    static NativeDigitalInputPin din(digital_in_path);
    static NativeDigitalOutputPin dout(digital_out_path);
    static NativeAnalogInputPin ain(analog_in_path);
    static NativeAnalogOutputPin aout(analog_out_path);
    digital_in = &din;
    digital_out = &dout;
    analog_in = &ain;
    analog_out = &aout;
    return digital_in->valid() && digital_out->valid() && analog_in->valid() && analog_out->valid();
}
#endif

static void setup_tasks() {
    event_loop.on_repeat(500, []() {
        const bool d = digital_in && digital_in->read();
        const int a = analog_in ? analog_in->read() : 0;
        print_text("digital in: ");
        print_text(d ? "1" : "0");
        print_text(" analog in: ");
        print_number(a);
        print_text("\n");
    });

    event_loop.on_repeat(1000, []() {
        digital_out_level = !digital_out_level;
        if (digital_out) {
            digital_out->write(digital_out_level);
        }
        if (analog_out) {
            analog_out->write(analog_out_value);
        }
        const int analog_max = analog_out ? analog_out->max_value() : 255;
        print_text("digital out: ");
        print_text(digital_out_level ? "1" : "0");
        print_text(" analog out: ");
        print_number(analog_out_value);
        print_text("\n");
        analog_out_value += analog_max / 8;
        if (analog_out_value > analog_max) {
            analog_out_value = 0;
        }
    });
}

#if defined(ARDUINO)
void setup() {
    Serial.begin(115200);
    if (!setup_pins()) {
        print_text("pin io setup failed\n");
        setup_failed = true;
        return;
    }
    setup_tasks();
}

void loop() {
    if (setup_failed) {
        print_text("pin io setup failed\n");
        delay(1000);
        return;
    }
    event_loop.tick();
}
#else
int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "usage: pin_io_example DIGITAL_IN_FILE DIGITAL_OUT_FILE ANALOG_IN_FILE ANALOG_OUT_FILE" << std::endl;
        return 2;
    }
    if (!setup_pins(argv[1], argv[2], argv[3], argv[4])) {
        std::cerr << "pin io setup failed" << std::endl;
        return 1;
    }
    setup_tasks();
    event_loop.run_forever();
    return 0;
}
#endif
