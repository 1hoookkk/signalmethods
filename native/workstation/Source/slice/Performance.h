#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace headspace {

class Performance {
public:
    bool message(std::uint32_t data) {
        const auto status = data & 255;
        if ((status & 15) != 0) return false;
        const auto key = (data >> 8) & 127;
        const auto value = (data >> 16) & 127;
        switch (status & 240) {
        case 144:
            if (value) {
                held_[key] = true; velocities_[key] = value; order_[key] = ++serial_;
                strikes += value / 127.;
                active = true; choose(); return true;
            }
            [[fallthrough]];
        case 128:
            held_[key] = false;
            if (!sustain_) order_[key] = 0;
            choose(); return true;
        case 176:
            if (key == 1) { morph = value / 127.f; return true; }
            if (key == 64) {
                sustain_ = value >= 64;
                if (!sustain_) for (unsigned i = 0; i < 128; ++i) if (!held_[i]) order_[i] = 0;
                choose(); return true;
            }
            if (key == 120 || key == 123) { release(); return true; }
            break;
        case 224:
            bend_ = (static_cast<int>(key + 128 * value) - 8192) / 8192.0 * 2;
            return true;
        }
        return false;
    }
    void release() { held_ = {}; order_ = {}; sustain_ = false; gate = false; }
    double hz() const { return 440 * std::exp2((note - 69 + bend_) / 12.); }
    std::size_t voices(std::array<double, 6>& out) const {
        std::size_t count = 0;
        for (unsigned key = 0; key < 128 && count < out.size(); ++key)
            if (order_[key]) out[count++] = 440 * std::exp2((static_cast<int>(key) - 69 + bend_) / 12.);
        return count;
    }
    bool active{}, gate{};
    int note{45};
    float velocity{1}, morph{}, q{};
    double strikes{};
private:
    void choose() {
        const auto latest = std::max_element(order_.begin(), order_.end());
        gate = *latest != 0;
        if (!gate) return;
        note = static_cast<int>(latest - order_.begin());
        velocity = velocities_[note] / 127.f;
        q = velocity;
    }
    std::array<bool, 128> held_{};
    std::array<unsigned, 128> velocities_{};
    std::array<std::uint64_t, 128> order_{};
    std::uint64_t serial_{};
    double bend_{};
    bool sustain_{};
};

}
