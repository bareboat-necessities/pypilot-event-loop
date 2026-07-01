#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <iostream>
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace async_event_loop_examples {

inline void print_text(const char* text) {
#if defined(ARDUINO)
    Serial.print(text);
#else
    std::cout << text;
#endif
}

inline void print_number(uint16_t value) {
#if defined(ARDUINO)
    Serial.print(value);
#else
    std::cout << value;
#endif
}

inline void print_size(size_t value) {
#if defined(ARDUINO)
    Serial.print(static_cast<unsigned long>(value));
#else
    std::cout << value;
#endif
}

inline void print_float(float value) {
    char text[32];
    snprintf(text, sizeof(text), "%.3f", static_cast<double>(value));
    print_text(text);
}

inline uint16_t printable_error_code(int error_code) {
    return static_cast<uint16_t>(error_code < 0 ? -error_code : error_code);
}

inline void print_line(const char* text) {
    print_text(text);
    print_text("\n");
}

inline void print_format_line(const char* format, ...) {
    char text[192];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    print_line(text);
}

template<typename Real = float>
struct DataValue {
    Real value = Real(0);
    uint64_t last_update_us = 0;
    bool valid = false;

    void set(Real new_value, uint64_t now_us) {
        value = new_value;
        last_update_us = now_us;
        valid = true;
    }
};

template<typename Real = float>
struct DataModel {
    DataValue<Real> apparent_wind_direction_rad;
    DataValue<Real> apparent_wind_speed_m_s;

    bool has_apparent_wind() const {
        return apparent_wind_direction_rad.valid && apparent_wind_speed_m_s.valid;
    }
};

using Real = float;

class NmeaTokenizer {
public:
    static constexpr int max_fields = 8;

    bool tokenize(char* text) {
        reset();
        if (!text) {
            return false;
        }

        char* field_start = text;
        while (field_count_ < max_fields) {
            fields_[field_count_++] = field_start;
            char* comma = strchr(field_start, ',');
            if (!comma) {
                break;
            }
            *comma = '\0';
            field_start = comma + 1;
        }
        return field_count_ > 0;
    }

    int field_count() const { return field_count_; }

    const char* field(int index) const {
        if (index < 0 || index >= field_count_ || !fields_[index]) {
            return "";
        }
        return fields_[index];
    }

private:
    void reset() {
        for (int i = 0; i < max_fields; ++i) {
            fields_[i] = nullptr;
        }
        field_count_ = 0;
    }

    char* fields_[max_fields]{};
    int field_count_ = 0;
};

inline bool parse_mwv(const NmeaTokenizer& tokenizer, DataModel<Real>& model, uint64_t now_us) {
    if (tokenizer.field_count() < 6 || strlen(tokenizer.field(0)) < 3) {
        return false;
    }

    const char* talker_sentence = tokenizer.field(0);
    const size_t sentence_len = strlen(talker_sentence);
    const char* sentence = sentence_len >= 3 ? talker_sentence + sentence_len - 3 : talker_sentence;
    if (strcmp(sentence, "MWV") != 0) {
        return false;
    }
    if (strcmp(tokenizer.field(2), "R") != 0 || tokenizer.field(5)[0] != 'A') {
        return false;
    }

    const char* angle_field = tokenizer.field(1);
    const char* speed_field = tokenizer.field(3);
    char* end_angle = nullptr;
    char* end_speed = nullptr;
    const Real angle_deg = static_cast<Real>(strtod(angle_field, &end_angle));
    const Real speed_value = static_cast<Real>(strtod(speed_field, &end_speed));
    if (end_angle == angle_field || end_speed == speed_field) {
        return false;
    }

    Real speed_factor = Real(1);
    if (strcmp(tokenizer.field(4), "N") == 0) {
        speed_factor = Real(0.514444);
    } else if (strcmp(tokenizer.field(4), "M") == 0) {
        speed_factor = Real(1);
    } else if (strcmp(tokenizer.field(4), "K") == 0) {
        speed_factor = Real(1000) / Real(3600);
    } else {
        return false;
    }

    model.apparent_wind_direction_rad.set(angle_deg * Real(3.14159265358979323846) / Real(180), now_us);
    model.apparent_wind_speed_m_s.set(speed_value * speed_factor, now_us);
    return true;
}

inline int format_signalk_wind_update(const DataModel<Real>& model,
                                       char* dst,
                                       size_t dst_size,
                                       const char* source_label = "mini-signalk") {
    if (!dst || dst_size == 0 || !model.has_apparent_wind()) {
        return 0;
    }

    return snprintf(
        dst,
        dst_size,
        "{\"updates\":[{\"source\":{\"label\":\"%s\"},"
        "\"values\":[{\"path\":\"environment.wind.angleApparent\",\"value\":%.6f},"
        "{\"path\":\"environment.wind.speedApparent\",\"value\":%.6f}]}]}\n",
        source_label ? source_label : "mini-signalk",
        static_cast<double>(model.apparent_wind_direction_rad.value),
        static_cast<double>(model.apparent_wind_speed_m_s.value));
}

} // namespace async_event_loop_examples
