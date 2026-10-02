#pragma once

#include "Model.h"
#include <algorithm>

namespace headspace {

enum class Pairing { kNatural, kCross, kInvert };

class Candidate {
public:
    void freeze(const Endpoint& words) { original_ = words; aimed_ = false; edited_ = false; derive(); }
    void aim(const Endpoint& words) { destination_ = canonical(words); aimed_ = true; edited_ = false; derive(); }
    void setAmount(float value) { amount_ = std::clamp(value, 0.f, 1.f); edited_ = false; derive(); }
    void adopt(const Endpoint& words) { words_ = canonical(words); edited_ = true; }
    void detach() { aimed_ = false; edited_ = false; derive(); }
    void setPairing(Pairing pairing) { pairing_ = pairing; edited_ = false; derive(); }
    Pairing pairing() const { return pairing_; }
    const Endpoint& words() const { return words_; }
    const Endpoint& original() const { return original_; }
    const Endpoint& destination() const { return destination_; }
    Endpoint target() const { return paired(destination_); }
    float amount() const { return amount_; }
    bool aimed() const { return aimed_; }
    bool edited() const { return edited_; }
    bool operator==(const Candidate&) const = default;

private:
    Endpoint paired(const Endpoint& words) const {
        if (pairing_ == Pairing::kCross) return reorder(words, {1, 0, 2, 3, 4, 5});
        if (pairing_ == Pairing::kInvert) return reorder(words, {5, 4, 3, 2, 1, 0});
        return words;
    }
    void derive() {
        if (!aimed_) { words_ = original_; return; }
        const auto target = paired(destination_);
        const auto words = packed({original_, target, original_, target}).interpolate_words(amount_, 0, 0);
        std::copy_n(words.begin(), 6, words_.begin());
    }
    Endpoint original_{}, destination_{}, words_{};
    Pairing pairing_{};
    float amount_{};
    bool aimed_{}, edited_{};
};

}
