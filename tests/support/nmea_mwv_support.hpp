#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace pypilot_event_loop_test {

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
};

using Real = float;

constexpr Real knots_to_m_s = Real(0.514444);
constexpr Real deg_to_rad = Real(3.14159265358979323846) / Real(180);

class NmeaTokenizer {
public:
    static constexpr int max_fields = 8;

    bool tokenize(char* text) {
        reset();
        if (!text) {
            return false;
        }

        char* save = nullptr;
        for (char* token = strtok_r(text, ",", &save);
             token && field_count_ < max_fields;
             token = strtok_r(nullptr, ",", &save)) {
            fields_[field_count_++] = token;
        }
        return field_count_ > 0;
    }

    int field_count() const { return field_count_; }

    const char* field(int index) const {
        if (index < 0 || index >= field_count_) {
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
    if (tokenizer.field_count() < 6 || std::strlen(tokenizer.field(0)) < 6) {
        return false;
    }
    const char* sentence = tokenizer.field(0) + std::strlen(tokenizer.field(0)) - 3;
    if (std::strcmp(sentence, "MWV") != 0 || std::strcmp(tokenizer.field(2), "R") != 0) {
        return false;
    }
    if (tokenizer.field(5)[0] != 'A') {
        return false;
    }

    const char* angle_field = tokenizer.field(1);
    const char* speed_field = tokenizer.field(3);
    char* end_angle = nullptr;
    char* end_speed = nullptr;
    const Real angle_deg = static_cast<Real>(std::strtod(angle_field, &end_angle));
    const Real speed_value = static_cast<Real>(std::strtod(speed_field, &end_speed));
    if (end_angle == angle_field || end_speed == speed_field) {
        return false;
    }

    Real factor = Real(1);
    if (std::strcmp(tokenizer.field(4), "N") == 0) {
        factor = knots_to_m_s;
    } else if (std::strcmp(tokenizer.field(4), "M") == 0) {
        factor = Real(1);
    } else if (std::strcmp(tokenizer.field(4), "K") == 0) {
        factor = Real(1000) / Real(3600);
    } else {
        return false;
    }

    model.apparent_wind_direction_rad.set(angle_deg * deg_to_rad, now_us);
    model.apparent_wind_speed_m_s.set(speed_value * factor, now_us);
    return true;
}

} // namespace pypilot_event_loop_test
