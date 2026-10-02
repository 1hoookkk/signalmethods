#pragma once
#include "Model.h"
#include <algorithm>
#include <stdexcept>

namespace headspace {
class Audition {
public:
    bool operator==(const Audition&) const = default;
    explicit Audition(const Model& model) { follow(model, 0, 0, false); }
    void follow(const Model& model, float morph, float q, bool stageMode) {
        if (!stageMode || q != q_ || !edgeEdited_) {
            edge_ = {model.body(0, q), model.body(1, q)};
            edgeEdited_ = false;
        }
        morph_ = morph; q_ = q;
        words_ = edgeEdited_ ? blend(edge_, std::clamp(morph, 0.f, 1.f)) : model.extended(morph, q, &reached_);
        anchor_ = words_; name_ = "Body"; detached_ = false;
        scale = 0; tension = .5f; stress = 1; tilt = 0;
    }
    void pick(const Template& source) {
        words_ = anchor_ = source.words; name_ = source.name;
        detached_ = true; edgeEdited_ = false;
        scale = 0; tension = .5f; stress = 1; tilt = 0;
    }
    void enterStage() {
        if (detached_) { edge_ = {words_, words_}; edgeEdited_ = true; detached_ = false; }
    }
    void shapeTo(float newScale, float newTension, float newStress, float newTilt) {
        auto words = shape(anchor_, newScale, newTension, newStress, newTilt, selection_);
        words_ = words; scale = newScale; tension = newTension; stress = newStress; tilt = newTilt;
        detached_ = true;
    }
    void resetShape() { shapeTo(0, .5f, 1, 0); }
    void editStage(std::size_t end, std::size_t stage, Pole value, unsigned selection = 0) {
        auto endpoints = edge_;
        Model scratch({"Stage", endpoints.at(end)});
        const auto reference = pole(endpoints.at(end), stage);
        if (!selection) selection = 1u << stage;
        for (std::size_t index = 0; index < 6; ++index) if (selection & (1u << index)) {
            const auto original = pole(endpoints[end], index);
            scratch.editPole(index, {original.hz * value.hz / reference.hz, original.bandwidth * value.bandwidth / reference.bandwidth});
        }
        endpoints[end] = scratch.corners()[0];
        const auto result = blend(endpoints, morph_);
        if (!compatible(result)) throw std::invalid_argument("Stage sweep does not survive quantization");
        edge_ = endpoints; words_ = anchor_ = result;
        edgeEdited_ = true; detached_ = false; name_ = "Stage " + std::to_string(stage + 1);
        scale = 0; tension = .5f; stress = 1; tilt = 0;
    }
    void editStageZero(std::size_t end, std::size_t stage, Zero value, unsigned selection = 0) {
        auto endpoints = edge_;
        if (!selection) selection = 1u << stage;
        for (std::size_t index = 0; index < 6; ++index)
            if (selection & (1u << index)) endpoints.at(end) = withZero(endpoints.at(end), index, value);
        const auto result = blend(endpoints, morph_);
        if (!compatible(result)) throw std::invalid_argument("Zero sweep does not survive quantization");
        edge_ = endpoints; words_ = anchor_ = result;
        edgeEdited_ = true; detached_ = false; name_ = "Zero " + std::to_string(stage + 1);
        scale = 0; tension = .5f; stress = 1; tilt = 0;
    }
    void editStageLive(std::size_t stage, Pole value, unsigned selection = 0) {
        const auto live = pole(words_, stage);
        if (!(live.hz > 0) || !(live.bandwidth > 0)) throw std::invalid_argument("Live stage has no usable pole");
        if (!selection) selection = 1u << stage;
        auto endpoints = edge_;
        for (std::size_t end = 0; end < 2; ++end) {
            Model scratch({"Stage", endpoints[end]});
            for (std::size_t index = 0; index < 6; ++index) if (selection & (1u << index)) {
                const auto original = pole(endpoints[end], index);
                scratch.editPole(index, {original.hz * value.hz / live.hz,
                    original.bandwidth * value.bandwidth / live.bandwidth});
            }
            endpoints[end] = scratch.corners()[0];
        }
        const auto result = blend(endpoints, morph_);
        if (!compatible(result)) throw std::invalid_argument("Live stage edit does not survive quantization");
        edge_ = endpoints; words_ = anchor_ = result;
        edgeEdited_ = true; detached_ = false; name_ = "Live " + std::to_string(stage + 1);
        scale = 0; tension = .5f; stress = 1; tilt = 0;
    }
    void reorderEdge(std::size_t end, const Permutation& permutation) {
        auto endpoints = edge_;
        endpoints.at(end) = reorder(endpoints.at(end), permutation);
        const auto result = blend(endpoints, morph_);
        if (!compatible(result)) throw std::invalid_argument("Topology does not survive quantization");
        edge_ = endpoints; words_ = anchor_ = result;
        edgeEdited_ = true; detached_ = false; name_ = "Topology";
        scale = 0; tension = .5f; stress = 1; tilt = 0;
    }
    float reached() const { return reached_; }
    unsigned selection() const { return selection_; }
    void selectPoles(unsigned selection) {
        selection &= 0x3f;
        if (!selection || selection == selection_) return;
        selection_ = selection; anchor_ = words_;
        scale = 0; tension = .5f; stress = 1; tilt = 0;
    }
    const Endpoint& words() const { return words_; }
    const Endpoint& edge(std::size_t end) const { return edge_.at(end); }
    const std::string& name() const { return name_; }
    bool detached() const { return detached_; }
    bool hasDraftEdge() const { return edgeEdited_; }
    float scale{}, tension{.5f}, stress{1}, tilt{};
private:
    static Endpoint blend(const std::array<Endpoint, 2>& edge, float morph) {
        const auto words = packed({edge[0], edge[1], edge[0], edge[1]}).interpolate_words(morph, 0, 0);
        Endpoint endpoint; std::copy_n(words.begin(), 6, endpoint.begin()); return endpoint;
    }
    Endpoint words_{}, anchor_{};
    std::array<Endpoint, 2> edge_{};
    std::string name_;
    float morph_{}, q_{};
    bool detached_{}, edgeEdited_{};
    unsigned selection_{0x3f};
    float reached_{};
};
}
