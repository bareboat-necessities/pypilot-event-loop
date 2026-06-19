#if defined(ARDUINO)
#include <Arduino.h>
#ifndef PYPILOT_DIGITAL_IN_PIN
#define PYPILOT_DIGITAL_IN_PIN 4
#endif
#ifndef PYPILOT_DIGITAL_OUT_PIN
#define PYPILOT_DIGITAL_OUT_PIN 5
#endif
#ifndef PYPILOT_ANALOG_IN_PIN
#define PYPILOT_ANALOG_IN_PIN A0
#endif
#ifndef PYPILOT_ANALOG_OUT_PIN
#define PYPILOT_ANALOG_OUT_PIN 6
#endif
#else
#include <cstdlib>
#include <iostream>
#endif

#include <pypilot_event_loop.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;
bool digital_out_level = false;
int analog_out_value = 0;

#if defined(ARDUINO)
NativeDigitalInputPin digital_in(PYPILOT_DIGITAL_IN_PIN, DigitalPinMode::InputPullup);
NativeDigitalOutputPin digital_out(PYPILOT_DIGITAL_OUT_PIN);
NativeAnalogInputPin analog_in(PYPILOT_ANALOG_IN_PIN);
NativeAnalogOutputPin analog_out(PYPILOT_ANALOG_OUT_PIN);
#else
NativeDigitalInputPin* digital_in = nullptr;
NativeDigitalOutputPin* digital_out = nullptr;
NativeAnalogInputPin* analog_in = nullptr;
NativeAnalogOutputPin* analog_out = nullptr;
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

static void setup_tasks() {
    event_loop.on_repeat(500, []() {
#if defined(ARDUINO)
        const bool d = digital_in.read();
        const int a = analog_in.read();
#else
        const bool d = digital_in && digital_in->read();
        const int a = analog_in ? analog_in->read() : 0;
#endif
        print_text("digital in: ");
        print_text(d ? "1" : "0");
        print_text(" analog in: ");
        print_number(a);
        print_text("\n");
    });

    event_loop.on_repeat(1000, []() {
        digital_out_level = !digital_out_level;
#if defined(ARDUINO)
        digital_out.write(digital_out_level);
        analog_out.write(analog_out_value);
        const int analog_max = analog_out.max_value();
#else
        if (digital_out) {
            digital_out->write(digital_out_level);
        }
        if (analog_out) {
            analog_out->write(analog_out_value);
        }
        const int analog_max = analog_out ? analog_out->max_value() : 255;
#endif
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
    setup_tasks();
}

void loop() {
    event_loop.tick();
}
#else
int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "usage: pin_io_example DIGITAL_IN_FILE DIGITAL_OUT_FILE ANALOG_IN_FILE ANALOG_OUT_FILE" << std::endl;
        return 2;
    }
    NativeDigitalInputPin digital_in_file(argv[1]);
    NativeDigitalOutputPin digital_out_file(argv[2]);
    NativeAnalogInputPin analog_in_file(argv[3]);
    NativeAnalogOutputPin analog_out_file(argv[4]);
    digital_in = &digital_in_file;
    digital_out = &digital_out_file;
    analog_in = &analog_in_file;
    analog_out = &analog_out_file;
    setup_tasks();
    event_loop.run_forever();
    return 0;
}
#endif
