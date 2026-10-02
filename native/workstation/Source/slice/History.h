#pragma once
#include <vector>

namespace headspace {
template<class State> class History {
public:
    void commit(const State& before, const State& after) {
        if (before == after) return;
        undo_.push_back(before); redo_.clear();
        if (undo_.size() > 128) undo_.erase(undo_.begin());
    }
    bool undo(State& state) {
        if (undo_.empty()) return false;
        redo_.push_back(state); state = undo_.back(); undo_.pop_back(); return true;
    }
    bool redo(State& state) {
        if (redo_.empty()) return false;
        undo_.push_back(state); state = redo_.back(); redo_.pop_back(); return true;
    }
private:
    std::vector<State> undo_, redo_;
};
}
