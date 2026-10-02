#pragma once

#include "Model.h"
#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace headspace {

struct Source {
    std::string name;
    std::string group;
    std::array<Pole, 6> poles;
    Endpoint words;
};

struct SourceSet {
    std::vector<Source> entries;
    std::vector<std::string> refused;
};

std::optional<std::array<Pole, 6>> resolvePoles(std::span<const Pole> own, std::string& reason);
SourceSet sourceSet();
std::optional<Source> sourceFromAudio(const std::filesystem::path& path, std::string& reason);

}
