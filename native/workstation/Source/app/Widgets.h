#pragma once

#include "imgui.h"
#include <cmath>

namespace headspace::ui {

constexpr auto ink = IM_COL32(255, 255, 255, 255);
constexpr auto accent = ink;
constexpr auto safe = IM_COL32(0, 255, 132, 255);
constexpr auto warn = IM_COL32(255, 224, 0, 255);
constexpr auto alarm = IM_COL32(255, 40, 40, 255);
constexpr auto trace = IM_COL32(0, 240, 255, 255);
constexpr auto peakTag = IM_COL32(204, 255, 0, 255);
constexpr auto zeroTag = IM_COL32(255, 0, 170, 255);
constexpr auto reticle = IM_COL32(26, 35, 51, 255);
constexpr auto padEdge = IM_COL32(20, 24, 32, 255);
constexpr auto frozen = IM_COL32(255, 196, 0, 255);
constexpr auto temporary = IM_COL32(255, 0, 170, 255);
constexpr auto grid = IM_COL32(34, 36, 38, 255);
constexpr auto muted = IM_COL32(132, 138, 144, 255);
constexpr auto live = accent;
constexpr auto ground = IM_COL32(0, 0, 0, 255);

inline ImFont* osdMono{};

inline void osd(ImDrawList* draw, ImVec2 position, ImU32 color, const char* text) {
    draw->AddText(osdMono, osdMono->LegacySize, position, color, text);
}

inline ImVec2 osdSize(const char* text) {
    return osdMono->CalcTextSizeA(osdMono->LegacySize, FLT_MAX, 0, text);
}

inline void diamond(ImDrawList* draw, ImVec2 center, float radius, ImU32 color) {
    const ImVec2 points[4]{ImVec2(center.x, center.y - radius), ImVec2(center.x + radius, center.y),
        ImVec2(center.x, center.y + radius), ImVec2(center.x - radius, center.y)};
    draw->AddPolyline(points, 4, color, ImDrawFlags_Closed, 1.f);
}

inline void dashed(ImDrawList* draw, ImVec2 from, ImVec2 to, ImU32 color) {
    const float dx = to.x - from.x, dy = to.y - from.y, length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0) return;
    for (float at = 0; at < length; at += 7) {
        const float t0 = at / length, t1 = std::min(at + 3.5f, length) / length;
        draw->AddLine(ImVec2(from.x + dx * t0, from.y + dy * t0), ImVec2(from.x + dx * t1, from.y + dy * t1), color, 1.f);
    }
}

inline bool hit(const char* id, ImVec2 position, ImVec2 size) {
    ImGui::SetCursorScreenPos(position);
    return ImGui::InvisibleButton(id, size);
}

inline bool chip(const char* id, const char* label, ImVec2 position, ImVec2 size, bool active = false) {
    const bool clicked = hit(id, position, size);
    auto* draw = ImGui::GetWindowDrawList();
    if (active || ImGui::IsItemHovered())
        draw->AddRect(position, ImVec2(position.x + size.x, position.y + size.y), active ? accent : muted, 0.f, 1.f);
    const auto text = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(position.x + (size.x - text.x) / 2, position.y + (size.y - text.y) / 2), active ? accent : ink, label);
    if (active) draw->AddLine(ImVec2(position.x + 8, position.y + size.y - 1), ImVec2(position.x + size.x - 8, position.y + size.y - 1), accent);
    return clicked;
}

inline void card(ImVec2 start, ImVec2 size, const char* title) {
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, ImVec2(start.x + size.x, start.y + size.y), ground, 4);
    draw->AddRect(start, ImVec2(start.x + size.x, start.y + size.y), grid, 4);
    draw->AddLine(ImVec2(start.x, start.y + 26), ImVec2(start.x + size.x, start.y + 26), grid);
    draw->AddText(ImVec2(start.x + 10, start.y + 5), ink, title);
}

}
