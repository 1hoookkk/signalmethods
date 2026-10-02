#include "Audio.h"
#include "MidiInput.h"
#include "Performance.h"
#include "Candidate.h"
#include "History.h"
#include "Response.h"
#include "Sources.h"
#include "trench/core/section_param.hpp"
#include <optional>
#include <bit>
#include <windows.h>
#include <commdlg.h>
#include <d3d11.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <memory>
#include <numbers>
#include <string>

namespace {

ID3D11Device* device{};
ID3D11DeviceContext* context{};
IDXGISwapChain* swapChain{};
ID3D11RenderTargetView* renderTarget{};
UINT resizeWidth{}, resizeHeight{};
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
ImFont* osdMono{};

void osd(ImDrawList* draw, ImVec2 position, ImU32 color, const char* text) {
    draw->AddText(osdMono, osdMono->LegacySize, position, color, text);
}

ImVec2 osdSize(const char* text) {
    return osdMono->CalcTextSizeA(osdMono->LegacySize, FLT_MAX, 0, text);
}

void diamond(ImDrawList* draw, ImVec2 center, float radius, ImU32 color) {
    const ImVec2 points[4]{ImVec2(center.x, center.y - radius), ImVec2(center.x + radius, center.y),
        ImVec2(center.x, center.y + radius), ImVec2(center.x - radius, center.y)};
    draw->AddPolyline(points, 4, color, ImDrawFlags_Closed, 1.f);
}

void dashed(ImDrawList* draw, ImVec2 from, ImVec2 to, ImU32 color) {
    const float dx = to.x - from.x, dy = to.y - from.y, length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0) return;
    for (float at = 0; at < length; at += 7) {
        const float t0 = at / length, t1 = std::min(at + 3.5f, length) / length;
        draw->AddLine(ImVec2(from.x + dx * t0, from.y + dy * t0), ImVec2(from.x + dx * t1, from.y + dy * t1), color, 1.f);
    }
}

constexpr auto grid = IM_COL32(34, 36, 38, 255);
constexpr auto muted = IM_COL32(132, 138, 144, 255);
constexpr auto live = accent;
constexpr auto ground = IM_COL32(0, 0, 0, 255);

bool hit(const char* id, ImVec2 position, ImVec2 size) {
    ImGui::SetCursorScreenPos(position);
    return ImGui::InvisibleButton(id, size);
}

bool chip(const char* id, const char* label, ImVec2 position, ImVec2 size, bool active = false) {
    const bool clicked = hit(id, position, size);
    auto* draw = ImGui::GetWindowDrawList();
    if (active || ImGui::IsItemHovered())
        draw->AddRect(position, ImVec2(position.x + size.x, position.y + size.y), active ? accent : muted, 0.f, 1.f);
    const auto text = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(position.x + (size.x - text.x) / 2, position.y + (size.y - text.y) / 2), active ? accent : ink, label);
    if (active) draw->AddLine(ImVec2(position.x + 8, position.y + size.y - 1), ImVec2(position.x + size.x - 8, position.y + size.y - 1), accent);
    return clicked;
}

void createRenderTarget() {
    ID3D11Texture2D* buffer{};
    if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) {
        device->CreateRenderTargetView(buffer, nullptr, &renderTarget);
        buffer->Release();
    }
}

void cleanup() {
    if (renderTarget) renderTarget->Release();
    if (swapChain) swapChain->Release();
    if (context) context->Release();
    if (device) device->Release();
}

void capture(const char* filename) {
    ID3D11Texture2D* buffer{};
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) throw std::runtime_error("Cannot capture frame");
    D3D11_TEXTURE2D_DESC desc;
    buffer->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    ID3D11Texture2D* staging{};
    const auto created = device->CreateTexture2D(&desc, nullptr, &staging);
    if (FAILED(created)) { buffer->Release(); throw std::runtime_error("Cannot stage frame"); }
    context->CopyResource(staging, buffer); buffer->Release();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) { staging->Release(); throw std::runtime_error("Cannot read frame"); }
    BITMAPFILEHEADER header{};
    header.bfType = 0x4D42; header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + desc.Width * desc.Height * 4;
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info); info.biWidth = desc.Width; info.biHeight = -static_cast<LONG>(desc.Height);
    info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&info), sizeof(info));
    for (UINT y = 0; y < desc.Height; ++y) {
        const auto* row = static_cast<const unsigned char*>(mapped.pData) + mapped.RowPitch * y;
        for (UINT x = 0; x < desc.Width; ++x) {
            const char pixel[]{static_cast<char>(row[x * 4 + 2]), static_cast<char>(row[x * 4 + 1]),
                static_cast<char>(row[x * 4]), static_cast<char>(row[x * 4 + 3])};
            file.write(pixel, 4);
        }
    }
    context->Unmap(staging, 0); staging->Release();
    if (!file) throw std::runtime_error("Cannot save captured frame");
}

bool createDevice(HWND window) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &desc, &swapChain, &device, &level, &context))) return false;
    createRenderTarget();
    return renderTarget != nullptr;
}

void audioCallback(ma_device* output, void* buffer, const void*, ma_uint32 frames) {
    auto& audio = *static_cast<headspace::Audio*>(output->pUserData);
    auto* destination = static_cast<float*>(buffer);
    std::array<float, 256> mono;
    while (frames > 0) {
        const auto count = std::min<std::size_t>(frames, mono.size());
        audio.process(std::span<float>(mono.data(), count));
        for (std::size_t i = 0; i < count; ++i) {
            *destination++ = mono[i];
            *destination++ = mono[i];
        }
        frames -= static_cast<ma_uint32>(count);
    }
}

constexpr int kKeyCentre = 45;
constexpr double kNarrowestVoiceHz = 9.0;
constexpr int kOverlayCount = 5;
constexpr const char* kOverlayName[kOverlayCount]{"OPEN", "MIN", "MAJ", "PENT", "PHRY"};
constexpr std::array<std::array<int, 6>, kOverlayCount> kOverlaySteps{{
    {{0, 0, 0, 0, 0, 0}},
    {{0, 3, 7, 12, 15, 19}},
    {{0, 4, 7, 12, 16, 19}},
    {{0, 2, 4, 7, 9, 12}},
    {{0, 1, 3, 5, 7, 8}}}};
constexpr const char* kFormantRole[6]{
    "jaw / pharynx", "tongue body / oral", "tongue tip / lip", "larynx / upper", "fixed upper", "fixed upper"};

void card(ImVec2 start, ImVec2 size, const char* title) {
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, ImVec2(start.x + size.x, start.y + size.y), ground, 4);
    draw->AddRect(start, ImVec2(start.x + size.x, start.y + size.y), grid, 4);
    draw->AddLine(ImVec2(start.x, start.y + 26), ImVec2(start.x + size.x, start.y + 26), grid);
    draw->AddText(ImVec2(start.x + 10, start.y + 5), ink, title);
}

struct App {
    headspace::Library library = headspace::loadLibrary(HEADSPACE_ROOT);
    headspace::SourceSet catalog = headspace::sourceSet();
    headspace::Model model{library.entries.at(0)};
    headspace::Candidate candidate;
    headspace::Audio audio;
    headspace::MidiInput midi;
    headspace::Performance performance;
    bool strikeMode{};
    double manualStrikes{};
    headspace::Resolved displayed;
    ma_device output{};
    double rate{48000.0};
    bool deviceReady{}, audioReady{}, initialized{}, playing{}, silentTest{}, controlAudition{};
    float strikeFlash{}, strikeVelocity{1};
    float flungX{}, flungY{}, shock{}, shockPhase{}, stampFlash{};
    std::array<float, 4> beacons{};
    ImVec2 stampFrom{};
    int stampTo{};
    bool puckHeld{};
    int overlay{};
    bool matchedLevel{true}, levelMatched{true};
    float aimTarget{};
    double seenStrikes{};
    char anchorFilter[64]{};
    bool polesLocked{}, zerosLocked{};
    std::size_t stage{}, dragSection{};
    float macroScale{}, macroStress{1}, macroTension{.5f}, macroTilt{}, macroSpan{1};
    headspace::Endpoint macroBase{};
    bool macroHeld{};
    float morph{}, q{};
    int first{}, second{1}, sourceTab{};
    headspace::Endpoint curveWords{};
    std::vector<headspace::ResponsePoint> curve;
    float minDb{-48}, maxDb{24};
    double audibleHi() const { return std::clamp(rate * .45, 8000., 16000.); }
    double dcDb{};
    float outputDb{-6.0f};
    const headspace::Endpoint* preview{};
    bool analyseRequested{};
    headspace::Endpoint levelWords{};
    double levelReference{1};
    std::string status, anchorName;
    bool statusAlarm{};

    struct EditState {
        headspace::Model model;
        headspace::Candidate candidate;
        float morph, q;
        float macroScale, macroStress, macroTension, macroTilt, macroSpan;
        bool macroHeld;
        headspace::Endpoint macroBase;
        bool operator==(const EditState&) const = default;
    };
    headspace::History<EditState> history;
    std::optional<EditState> gesture;
    bool historyAction{};
    EditState captureEdit() const {
        return {model, candidate, morph, q, macroScale, macroStress, macroTension, macroTilt, macroSpan, macroHeld, macroBase};
    }
    void restoreEdit(const EditState& edit) {
        model = edit.model; candidate = edit.candidate; morph = edit.morph; q = edit.q;
        macroScale = edit.macroScale; macroStress = edit.macroStress; macroSpan = edit.macroSpan;
        macroTension = edit.macroTension; macroTilt = edit.macroTilt;
        macroHeld = edit.macroHeld; macroBase = edit.macroBase;
        preview = nullptr;
    }
    void travelHistory(bool redo) {
        auto edit = captureEdit();
        if (redo ? history.redo(edit) : history.undo(edit)) restoreEdit(edit);
        gesture.reset(); historyAction = true; status.clear(); statusAlarm = false;
    }

    App() {
        auto config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 2;
        config.sampleRate = 0;
        config.periodSizeInFrames = 256;
        config.dataCallback = audioCallback;
        config.pUserData = &audio;
        deviceReady = ma_device_init(nullptr, &config, &output) == MA_SUCCESS;
        if (deviceReady && output.sampleRate > 0) rate = output.sampleRate;
        audio.prepare(rate);
        candidate.freeze(model.corners()[0]);
        displayed = headspace::resolve(candidate.words(), rate);
        audioReady = deviceReady && ma_device_start(&output) == MA_SUCCESS;
    }
    ~App() { if (deviceReady) ma_device_uninit(&output); }

    static std::string coordinate(std::size_t corner) {
        return std::string("M") + (corner % 2 ? "100" : "0") + " Q" + (corner / 2 ? "100" : "0");
    }

    void chooser(const char* label, int& index) {
        if (ImGui::BeginCombo(label, library.entries[index].name.c_str())) {
            for (std::size_t i = 0; i < library.entries.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(library.entries[i].name.c_str(), index == static_cast<int>(i))) index = static_cast<int>(i);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }

    void bootstrap() {
        chooser("First template", first);
        ImGui::Checkbox("Two Morph endpoints", &twoTemplates);
        if (twoTemplates) chooser("Second template", second);
        if (ImGui::Button("Initialize four endpoints", ImVec2(250, 38))) {
            model.bootstrap(library.entries[first], twoTemplates ? &library.entries[second] : nullptr);
            initialized = true;
            candidate.freeze(model.body(morph, q));
            ImGui::CloseCurrentPopup();
        }
    }
    bool twoTemplates{};

    std::string frozenLabel() const {
        char text[96];
        const bool onM = morph < .002f || morph > .998f, onQ = q < .002f || q > .998f;
        const std::size_t nearest = static_cast<std::size_t>((morph >= .5f ? 1 : 0) + (q >= .5f ? 2 : 0));
        if (onM && onQ) std::snprintf(text, sizeof(text), "M%.0f Q%.0f   %s", morph * 100, q * 100, model.names()[nearest].c_str());
        else std::snprintf(text, sizeof(text), "M%.2f Q%.2f   BLEND", morph, q);
        return text;
    }

    void freeze() {
        candidate.freeze(model.body(morph, q));
        rebase();
        status = "FROZEN " + frozenLabel(); statusAlarm = false;
    }

    void aim(const std::string& name, const headspace::Endpoint& words) {
        anchorName = name;
        candidate.aim(words);
        candidate.setAmount(0);
        aimTarget = 1.f;
        rebase();
        status = "ANCHOR " + name; statusAlarm = false;
    }

    void landed(std::size_t corner) {
        morph = static_cast<float>(corner % 2);
        q = static_cast<float>(corner / 2);
        model.selectAt(morph, q);
        candidate.freeze(model.body(morph, q));
        candidate.setAmount(0);
        rebase();
    }

    void stamp(std::size_t corner) {
        stampFrom = ImGui::GetIO().MousePos;
        stampTo = static_cast<int>(corner);
        stampFlash = 1.f;
        beacons[corner] = 1.f;
        try {
            auto words = candidate.words();
            const bool live = audioReady && !silentTest && (strikeMode || playing || performance.active || controlAudition);
            if (live) {
                const auto captured = audio.captureWords();
                if (!captured) throw std::runtime_error("Audio snapshot busy; stamp again");
                words = *captured;
            }
            model.setCorner(corner, words, live ? "Performance" : candidate.aimed() ? "Candidate" : "Frozen");
            status = "STAMPED " + coordinate(corner); statusAlarm = false;
            landed(corner);
        } catch (const std::exception& e) { status = e.what(); statusAlarm = true; }
    }

    void applyMacros() {
        if (!macroHeld) { macroBase = candidate.words(); macroHeld = true; }
        try {
            candidate.adopt(headspace::shape(macroBase, macroScale, macroTension, macroStress, macroTilt, 0x3f, macroSpan));
            status.clear(); statusAlarm = false;
        } catch (const std::exception& e) { status = e.what(); statusAlarm = true; }
    }

    void rebase() {
        macroHeld = false;
        macroScale = 0; macroStress = 1; macroTension = .5f; macroTilt = 0;
    }

    void applySection(std::size_t& section, const trench::core::p2k::SectionParam& param) {
        headspace::Endpoint settled;
        if (!headspace::compileSection(candidate.words(), section, param, 24.0, settled)) {
            status = "Section edit refused at the +24 dB ceiling"; statusAlarm = true;
            return;
        }
        if (settled == candidate.words()) return;
        candidate.adopt(settled);
        rebase();
        status.clear(); statusAlarm = false;
    }

    void applyZero(std::size_t& section, const headspace::Zero& zero) {
        headspace::Endpoint settled;
        if (!headspace::compileZero(candidate.words(), section, zero, settled)) {
            status = "Zero edit refused"; statusAlarm = true;
            return;
        }
        if (settled == candidate.words()) return;
        candidate.adopt(settled);
        rebase();
        status.clear(); statusAlarm = false;
    }

    void response(ImVec2 start, ImVec2 size, bool diagnostics, bool chrome = true) {
        auto* draw = ImGui::GetWindowDrawList();
        if (chrome) {
            draw->AddText(ImVec2(start.x, start.y - 30), ink, "CASCADE / DC");
            char caption[96];
            std::snprintf(caption, sizeof(caption), "%s", preview ? "PREVIEW" : candidate.edited() ? "EDITED" : "LIVE");
            draw->AddText(ImVec2(start.x + size.x - ImGui::CalcTextSize(caption).x, start.y - 30), live, caption);
        }
        const ImVec2 low = chrome ? ImVec2(start.x + 43, start.y + 12) : start;
        const ImVec2 high = chrome ? ImVec2(start.x + size.x - 18, start.y + size.y - 30)
                                   : ImVec2(start.x + size.x, start.y + size.y);
        if (curve.empty() || curveWords != displayed.words) {
            curve = headspace::responseCurve(displayed);
            curveWords = displayed.words;
            dcDb = trench::core::cascade_response_db(displayed.cascade, 0, trench::core::kP2kDatumHz);
        }
        const auto y = [&](double db) { return high.y - static_cast<float>((db - minDb) / (maxDb - minDb)) * (high.y - low.y); };
        const double decades = std::log10(audibleHi() / 20);
        const auto x = [&](double hz) { return low.x + static_cast<float>(std::log10(hz / 20) / decades) * (high.x - low.x); };
        draw->AddRectFilled(low, high, ground);
        for (float db = minDb; db <= maxDb; db += 12) {
            draw->AddLine(ImVec2(low.x, y(db)), ImVec2(high.x, y(db)), reticle, 1.f);
            if (!chrome) {
                if (db == 0) draw->AddLine(ImVec2(low.x, y(db)), ImVec2(high.x, y(db)), grid, 1.f);
                char mark[12]; std::snprintf(mark, sizeof(mark), "%+.0f", db);
                osd(draw, ImVec2(low.x + 6, y(db) - 14), reticle, mark);
                continue;
            }
            draw->AddLine(ImVec2(low.x, y(db)), ImVec2(low.x + 4, y(db)), muted);
            draw->AddLine(ImVec2(high.x - 4, y(db)), ImVec2(high.x, y(db)), muted);
            char label[20]; std::snprintf(label, sizeof(label), "%+.0f", db);
            osd(draw, ImVec2(start.x, y(db) - 7), muted, label);
        }
        for (const float hz : {20.f, 100.f, 1000.f, 10000.f}) {
            if (hz > audibleHi()) continue;
            draw->AddLine(ImVec2(x(hz), low.y), ImVec2(x(hz), high.y), reticle, 1.f);
            if (!chrome) {
                const char* mark = hz == 20 ? "20" : hz == 100 ? "100" : hz == 1000 ? "1K" : "10K";
                osd(draw, ImVec2(x(hz) + 5, high.y - 18), reticle, mark);
                continue;
            }
            draw->AddLine(ImVec2(x(hz), high.y - 4), ImVec2(x(hz), high.y), muted);
            const char* label = hz == 20 ? "20" : hz == 100 ? "100" : hz == 1000 ? "1K" : "10K";
            osd(draw, ImVec2(x(hz) - osdSize(label).x / 2, high.y + 9), muted, label);
        }
        if (chrome) draw->AddRect(low, high, reticle, 0.f, 1.f);
        for (int corner = 0; corner < 4 && chrome; ++corner) {
            const float cx = corner % 2 ? high.x : low.x, cy = corner / 2 ? high.y : low.y;
            const float sx = corner % 2 ? -9.f : 9.f, sy = corner / 2 ? -9.f : 9.f;
            draw->AddLine(ImVec2(cx, cy), ImVec2(cx + sx, cy), ink);
            draw->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + sy), ink);
        }
        draw->PushClipRect(low, high, true);
        for (std::size_t i = 1; i < curve.size(); ++i) {
            const ImVec2 a(x(curve[i - 1].hz), y(curve[i - 1].db - dcDb)), b(x(curve[i].hz), y(curve[i].db - dcDb));
            draw->AddLine(a, b, trace, 1.5f);
        }
        if (diagnostics) {
            std::array<std::size_t, 6> order{0, 1, 2, 3, 4, 5};
            std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
                return headspace::pole(displayed.words, a).hz < headspace::pole(displayed.words, b).hz;
            });
            std::size_t formant = 0;
            for (std::size_t rank = 0; rank < order.size(); ++rank) {
                const auto section = order[rank];
                const auto p = headspace::pole(displayed.words, section);
                if (!(p.hz > 0)) continue;
                const auto z = headspace::zeroOf(displayed.words, section);
                const bool highCut = !z.parked && z.hz >= p.hz * 4;
                if (!highCut) ++formant;
                const float px = x(p.hz), lane = low.y + 8 + rank * 19;
                const auto color = stage == section ? accent : muted;
                if (chrome) {
                    draw->AddLine(ImVec2(px, lane + 15), ImVec2(px, high.y), reticle, 1.f);
                    char label[48];
                    if (highCut) std::snprintf(label, sizeof(label), "S%d HC", static_cast<int>(section + 1));
                    else std::snprintf(label, sizeof(label), "S%d F%d", static_cast<int>(section + 1), static_cast<int>(formant));
                    const float labelX = std::clamp(px + 5, low.x, high.x - 46);
                    draw->AddRectFilled(ImVec2(labelX - 2, lane - 1), ImVec2(labelX + 44, lane + 17), ground, 2);
                    osd(draw, ImVec2(labelX, lane + 1), color, label);
                } else if (stage == section) draw->AddLine(ImVec2(px, low.y), ImVec2(px, high.y), reticle, 1.f);
                const double db = trench::core::cascade_response_db(displayed.cascade, p.hz, trench::core::kP2kDatumHz) - dcDb;
                const ImVec2 peak(px, y(db));
                diamond(draw, peak, stage == section ? 7.f : 5.f, peakTag);
                if (!z.parked && z.hz >= 20 && z.hz <= 20000) {
                    const double zdb = trench::core::cascade_response_db(displayed.cascade, z.hz, trench::core::kP2kDatumHz) - dcDb;
                    const ImVec2 notch(x(z.hz), y(zdb));
                    draw->AddCircle(notch, stage == section ? 7.f : 5.f, zeroTag, 0, 1.f);
                    ImGui::PushID(static_cast<int>(section) + 100);
                    ImGui::SetCursorScreenPos(ImVec2(notch.x - 10, notch.y - 10));
                    ImGui::InvisibleButton("zero", ImVec2(20, 20));
                    if (ImGui::IsItemActive() && !zerosLocked) {
                        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
                        auto& io = ImGui::GetIO();
                        float hz = static_cast<float>(z.hz), bw = static_cast<float>(z.bandwidth);
                        if (io.MouseDelta.x != 0)
                            hz = static_cast<float>(std::clamp(20.0 * std::pow(1000.0,
                                std::clamp((io.MousePos.x - low.x) / (high.x - low.x), 0.f, 1.f)), 20., 18000.));
                        if (io.MouseWheel != 0) bw = std::clamp(bw * std::pow(1.1f, io.MouseWheel), 20.f, 8000.f);
                        std::size_t target = section;
                        applyZero(target, {hz, bw, false});
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zero %.0f Hz, BW %.0f\nDrag X: frequency   Wheel: width   Shift-click the diamond: park",
                        z.hz, z.bandwidth);
                    ImGui::PopID();
                }
                ImGui::PushID(static_cast<int>(section));
                ImGui::SetCursorScreenPos(ImVec2(peak.x - 10, peak.y - 10));
                ImGui::InvisibleButton("peak", ImVec2(20, 20));
                const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
                if (ImGui::IsItemActivated()) dragSection = section;
                if (hovered || active) stage = active ? dragSection : section;
                if (hovered && !zerosLocked && ImGui::IsMouseClicked(0) && ImGui::GetIO().KeyShift) {
                    const auto now = headspace::pole(displayed.words, section);
                    std::size_t target = section;
                    applyZero(target, z.parked ? headspace::Zero{now.hz, std::max(now.bandwidth * .5, 60.), false}
                                               : headspace::Zero{0, 0, true});
                } else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    auto param = trench::core::p2k::param_of(displayed.words[section], trench::core::kP2kDatumHz);
                    param.type = param.type == trench::core::p2k::SectionType::kOff
                        ? trench::core::p2k::SectionType::kLowPass
                        : param.type == trench::core::p2k::SectionType::kLowPass
                            ? trench::core::p2k::SectionType::kEq
                            : trench::core::p2k::SectionType::kOff;
                    std::size_t target = section;
                    applySection(target, param);
                } else if (active) {
                    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
                    auto& io = ImGui::GetIO();
                    auto param = trench::core::p2k::param_of(displayed.words[dragSection], trench::core::kP2kDatumHz);
                    if (ImGui::IsMouseDoubleClicked(0))
                        param.type = param.type == trench::core::p2k::SectionType::kLowPass
                            ? trench::core::p2k::SectionType::kEq
                            : trench::core::p2k::SectionType::kLowPass;
                    else {
                        if (io.MouseDelta.x != 0 && !polesLocked)
                            param.fc_hz = std::clamp(20.0 * std::pow(1000.0,
                                std::clamp((io.MousePos.x - low.x) / (high.x - low.x), 0.f, 1.f)), 20., 18000.);
                        if (io.MouseDelta.y != 0 && !polesLocked)
                            param.bw_oct = std::clamp(param.bw_oct * std::exp(io.MouseDelta.y * .004), .05, 6.);
                        if (io.MouseWheel != 0 && !polesLocked) param.bw_oct = std::clamp(param.bw_oct * std::pow(1.1f, io.MouseWheel), .05, 6.);
                    }
                    applySection(dragSection, param);
                }
                char role[64];
                if (highCut) std::snprintf(role, sizeof(role), "reserved high-cut");
                else std::snprintf(role, sizeof(role), "F%d, %s", static_cast<int>(formant),
                    kFormantRole[std::min<std::size_t>(formant - 1, 5)]);
                if (hovered) ImGui::SetTooltip("S%d = %s\n%.1f Hz, BW %.1f Hz\nDrag X: frequency   Drag Y: gain\nWheel: bandwidth   Double-click: Eq/LowPass   Right-click: cycle incl. Off\nShift-click: unpark/park the zero",
                    static_cast<int>(section + 1), role, p.hz, p.bandwidth);
                ImGui::PopID();
            }
        }
        draw->PopClipRect();
    }

    double ringSeconds() const {
        double slowest = 0;
        for (std::size_t s = 0; s < 6; ++s) {
            const auto p = headspace::pole(displayed.words, s);
            if (p.hz > 0 && p.bandwidth > 0) slowest = std::max(slowest, 1. / (std::numbers::pi * p.bandwidth));
        }
        return std::clamp(slowest, .04, 1.2);
    }

    void stagePlot(ImVec2 start, float size, std::size_t section) {
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 end(start.x + size, start.y + size);
        draw->AddRect(start, end, grid, 0, 0, 1.f);
        const auto own = headspace::pole(displayed.words, section);
        const double band = audibleHi();
        const double lo = own.hz > 0 ? std::max(20., own.hz / 6) : 20.;
        const double hi = own.hz > 0 ? std::min(band, own.hz * 6) : band;
        const double decades = std::log10(hi / lo);
        const auto ypos = [&](double db) {
            return start.y + size - static_cast<float>((db - minDb) / (maxDb - minDb)) * size;
        };
        const auto xpos = [&](double hz) {
            return start.x + static_cast<float>(std::log10(hz / lo) / decades) * size;
        };
        for (float db = minDb + 12; db < maxDb; db += 12)
            draw->AddLine(ImVec2(start.x, ypos(db)), ImVec2(end.x, ypos(db)),
                db == 0 ? grid : reticle, 1.f);
        if (own.hz > 0) draw->AddLine(ImVec2(xpos(own.hz), start.y), ImVec2(xpos(own.hz), end.y), reticle, 1.f);
        const auto& biquad = displayed.cascade[section];
        const double reference = trench::core::section_response_db(biquad, 0, displayed.rate);
        ImVec2 previous{};
        for (int i = 0; i <= 96; ++i) {
            const double hz = lo * std::pow(hi / lo, i / 96.);
            const double db = trench::core::section_response_db(biquad, hz, displayed.rate) - reference;
            const ImVec2 at(xpos(hz), ypos(std::clamp(db, static_cast<double>(minDb), static_cast<double>(maxDb))));
            if (i) draw->AddLine(previous, at, trace, 1.5f);
            previous = at;
        }
        char tag[8]; std::snprintf(tag, sizeof(tag), "S%d", static_cast<int>(section + 1));
        osd(draw, ImVec2(start.x + 5, start.y + 4), section == stage ? accent : muted, tag);
        char centre[24];
        if (own.hz > 0) std::snprintf(centre, sizeof(centre), "%.0f Hz", own.hz);
        else std::snprintf(centre, sizeof(centre), "off");
        osd(draw, ImVec2(start.x + 5, start.y + size - 18), muted, centre);
        const auto here = own;
        if (here.hz > 0) {
            double nearest = std::numeric_limits<double>::max();
            for (std::size_t t = 0; t < 6; ++t) {
                if (t == section) continue;
                const auto other = headspace::pole(displayed.words, t);
                if (other.hz > 0) nearest = std::min(nearest, std::abs(here.hz - other.hz));
            }
            if (nearest < headspace::kProximityHz) {
                char note[24]; std::snprintf(note, sizeof(note), "DAMPED %.0f", nearest);
                osd(draw, ImVec2(start.x + 5, start.y + size - 32), alarm, note);
            }
        }
        if (candidate.aimed()) {
            const auto from = headspace::pole(candidate.original(), section);
            const auto to = headspace::pole(candidate.target(), section);
            char travel[24];
            if (from.hz > 0 && to.hz > 0)
                std::snprintf(travel, sizeof(travel), "%+.1f st", 12 * std::log2(to.hz / from.hz));
            else std::snprintf(travel, sizeof(travel), "no pair");
            const float tw = osdSize(travel).x;
            osd(draw, ImVec2(start.x + size - tw - 5, start.y + 4),
                from.hz > 0 && to.hz > 0 ? frozen : alarm, travel);
        }
    }

    void membraneGrab(ImVec2 start, ImVec2 size) {
        const float dt = ImGui::GetIO().DeltaTime;
        ImGui::SetNextItemAllowOverlap();
        hit("membrane", start, size);
        const ImVec2 puck(start.x + morph * size.x, start.y + (1 - q) * size.y);
        if (ImGui::IsItemActivated()) {
            const auto m = ImGui::GetIO().MousePos;
            const float dx = m.x - puck.x, dy = m.y - puck.y;
            puckHeld = dx * dx + dy * dy < 28.f * 28.f;
        }
        if (!ImGui::IsItemActive()) puckHeld = false;
        const bool grabbing = ImGui::IsItemActive() && puckHeld;
        if (grabbing) {
            const auto& io = ImGui::GetIO();
            morph = std::clamp((io.MousePos.x - start.x) / size.x, 0.f, 1.f);
            q = std::clamp(1.f - (io.MousePos.y - start.y) / size.y, 0.f, 1.f);
        }
        if (grabbing) {
            model.selectAt(morph, q);
            candidate.freeze(model.body(morph, q));
            candidate.setAmount(0); aimTarget = 0;
            rebase();
            controlAudition = true;
        }
    }

    void membrane(ImVec2 start, ImVec2 size) {
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 end(start.x + size.x, start.y + size.y);
        const float dt = ImGui::GetIO().DeltaTime;
        shock *= std::exp(-dt / static_cast<float>(ringSeconds()));
        if (strikeFlash > .98f) { shock = 1.f; shockPhase = 0; }
        shockPhase += dt * (7.f + 26.f * strikeVelocity);
        const float amplitude = size.y * .07f * shock;
        const float ripple = 2.f + 9.f * strikeVelocity;
        response(start, size, false, false);
        if (shock > .01f) drawHorizon(start, size, amplitude, ripple);
        for (std::size_t c = 0; c < 4; ++c) {
            beacons[c] = std::max(0.f, beacons[c] - dt * 2.2f);
            const ImVec2 cell(start.x + (c % 2) * size.x, start.y + (1 - c / 2) * size.y);
            const bool here = model.selected() == c;
            const float glow = (here ? .55f : .18f) + beacons[c] * .45f;
            const float halo = 26.f + beacons[c] * 60.f;
            for (int ring = 0; ring < 3; ++ring) {
                const float r = halo * (1.f - ring * .3f);
                draw->AddCircle(cell, r, IM_COL32(0, 255, 132, static_cast<int>(255 * glow * (.3f - ring * .08f))), 0, 2.f);
            }
            draw->AddCircleFilled(cell, 4.f + beacons[c] * 5.f,
                IM_COL32(0, 255, 132, static_cast<int>(255 * std::min(1.f, glow + .3f))));
            if (!here) continue;
            const auto& name = model.names()[c];
            const float tw = ImGui::CalcTextSize(name.c_str()).x;
            draw->AddText(ImVec2(c % 2 == 0 ? cell.x + 16 : cell.x - tw - 16,
                cell.y + (c / 2 == 0 ? -28 : 14)), accent, name.c_str());
        }
        const ImVec2 position(start.x + morph * size.x, start.y + (1 - q) * size.y);
        const float speed = std::sqrt(flungX * flungX + flungY * flungY);
        draw->AddCircle(position, 10.f + shock * 26.f + speed * 6.f, ink, 0, 1.f + shock * 2.f);
        draw->AddCircleFilled(position, 2.5f, trace);
        if (stampFlash > 0) {
            stampFlash = std::max(0.f, stampFlash - dt * 3.2f);
            const ImVec2 target(start.x + (stampTo % 2) * size.x, start.y + (1 - stampTo / 2) * size.y);
            const float t = 1.f - stampFlash;
            draw->AddLine(stampFrom, ImVec2(stampFrom.x + (target.x - stampFrom.x) * t,
                stampFrom.y + (target.y - stampFrom.y) * t),
                IM_COL32(255, 196, 0, static_cast<int>(255 * stampFlash)), 2.f);
        }
    }

    void drawHorizon(ImVec2 start, ImVec2 size, float amplitude, float ripple) {
        auto* draw = ImGui::GetWindowDrawList();
        if (curve.empty() || curveWords != displayed.words) {
            curve = headspace::responseCurve(displayed);
            curveWords = displayed.words;
            dcDb = trench::core::cascade_response_db(displayed.cascade, 0, trench::core::kP2kDatumHz);
        }
        const auto ypos = [&](double db) {
            return start.y + size.y - static_cast<float>((db - minDb) / (maxDb - minDb)) * size.y;
        };
        const double decades = std::log10(audibleHi() / 20);
        const auto xpos = [&](double hz) { return start.x + static_cast<float>(std::log10(hz / 20) / decades) * size.x; };
        const auto lift = [&](float x) {
            const float u = (x - start.x) / size.x;
            return -amplitude * std::sin(6.2831853f * (ripple * u - shockPhase)) * std::sin(3.14159f * u);
        };
        ImVec2 previous(xpos(curve.front().hz), ypos(curve.front().db - dcDb));
        previous.y += lift(previous.x);
        for (std::size_t i = 1; i < curve.size(); ++i) {
            ImVec2 next(xpos(curve[i].hz), ypos(curve[i].db - dcDb));
            next.y += lift(next.x);
            draw->AddLine(previous, next, trace, 2.f);
            previous = next;
        }
    }

    void pad(ImVec2 start, float size) {
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 end(start.x + size, start.y + size);
        hit("runtime pad", start, ImVec2(size, size));
        if (ImGui::IsItemActive()) {
            const auto mouse = ImGui::GetIO().MousePos;
            morph = std::clamp((mouse.x - start.x) / size, 0.f, 1.f);
            q = std::clamp(1.f - (mouse.y - start.y) / size, 0.f, 1.f);
            model.selectAt(morph, q);
            candidate.freeze(model.body(morph, q));
            rebase();
            controlAudition = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("The four saved corners, played as the plugin plays them. Drag to morph; Shift+1-4 jumps to a corner.");
        draw->AddRectFilled(start, end, ground);
        for (int i = 1; i < 4; ++i) {
            dashed(draw, ImVec2(start.x + size * i / 4, start.y), ImVec2(start.x + size * i / 4, end.y), reticle);
            dashed(draw, ImVec2(start.x, start.y + size * i / 4), ImVec2(end.x, start.y + size * i / 4), reticle);
        }
        draw->AddRect(start, end, padEdge, 0.f, 1.f);
        const ImVec2 position(start.x + morph * size, start.y + (1 - q) * size);
        draw->AddLine(ImVec2(start.x, position.y), ImVec2(end.x, position.y), IM_COL32(186, 186, 186, 120), 1.5f);
        draw->AddLine(ImVec2(position.x, start.y), ImVec2(position.x, end.y), IM_COL32(186, 186, 186, 120), 1.5f);
        draw->AddCircle(position, 6.f, ink, 0, 1.f);
        draw->AddCircleFilled(position, 1.f, trace);
        for (std::size_t c = 0; c < 4; ++c) {
            const ImVec2 cell(start.x + (c % 2) * size, start.y + (1 - c / 2) * size);
            char label[64];
            std::snprintf(label, sizeof(label), "%s  %s", coordinate(c).c_str(), model.names()[c].c_str());
            const float tw = ImGui::CalcTextSize(label).x;
            const float x = c % 2 == 0 ? cell.x + 8 : cell.x - tw - 8;
            const float y = cell.y + (c / 2 == 0 ? 6 : -20);
            draw->AddRectFilled(ImVec2(x - 2, y - 1), ImVec2(x + tw + 2, y + 17), ground, 2);
            draw->AddText(ImVec2(x, y), muted, label);
        }
    }

    void sources(ImVec2 start, ImVec2 size) {
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(ImVec2(start.x - 14, start.y - 44), ImVec2(start.x + size.x + 14, start.y + size.y + 14),
            IM_COL32(10, 10, 12, 242), 4);
        draw->AddRect(ImVec2(start.x - 14, start.y - 44), ImVec2(start.x + size.x + 14, start.y + size.y + 14), grid, 4, 0, 1.f);
        draw->AddText(ImVec2(start.x, start.y - 30), ink, "ANCHOR");
        if (chip("source list", "SOURCES", ImVec2(start.x, start.y - 12), ImVec2(80, 26), sourceTab == 0)) sourceTab = 0;
        if (chip("corpus list", "CORPUS", ImVec2(start.x + 84, start.y - 12), ImVec2(80, 26), sourceTab == 1)) sourceTab = 1;
        if (chip("analyse wav", "ANALYSE WAV", ImVec2(start.x + 172, start.y - 12), ImVec2(104, 26))) analyseRequested = true;
        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + 20));
        ImGui::SetNextItemWidth(size.x);
        ImGui::InputTextWithHint("##anchor filter", "filter", anchorFilter, sizeof(anchorFilter));
        const auto passes = [&](const std::string& name) {
            if (!anchorFilter[0]) return true;
            std::string lower = name, needle = anchorFilter;
            for (auto& ch : lower) ch = static_cast<char>(std::tolower(ch));
            for (auto& ch : needle) ch = static_cast<char>(std::tolower(ch));
            return lower.find(needle) != std::string::npos;
        };
        const float row = 26;
        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + 52));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("Anchor list", ImVec2(size.x, size.y - 52), 0, ImGuiWindowFlags_NoBackground);
        const auto listStart = ImGui::GetCursorScreenPos();
        const float listWidth = ImGui::GetContentRegionAvail().x;
        auto* listDraw = ImGui::GetWindowDrawList();
        float y = listStart.y;
        const auto entry = [&](const std::string& name, const headspace::Endpoint& words) {
            const ImVec2 rowTop(listStart.x, y);
            ImGui::PushID(name.c_str());
            const bool clicked = hit("source", rowTop, ImVec2(listWidth, row - 2));
            const bool hovered = ImGui::IsItemHovered();
            if (hovered) preview = &words;
            if (clicked) aim(name, words);
            const bool aimed = candidate.aimed() && words == candidate.destination();
            if (hovered || aimed) listDraw->AddRectFilled(rowTop, ImVec2(rowTop.x + listWidth, rowTop.y + row - 2), ground, 4);
            listDraw->AddText(ImVec2(rowTop.x + 8, rowTop.y + 4), aimed ? temporary : hovered ? live : ink, name.c_str());
            ImGui::PopID();
            y += row;
        };
        if (sourceTab == 0) {
            std::string group;
            for (const auto& source : catalog.entries) {
                if (!passes(source.name) && !passes(source.group)) continue;
                if (source.group != group) {
                    group = source.group;
                    if (y > listStart.y) y += row * .5f;
                    listDraw->AddText(ImVec2(listStart.x + 8, y + 4), muted, group.c_str());
                    y += row;
                }
                entry(source.name, source.words);
            }
        } else {
            for (const auto& t : library.entries) {
                if (t.acoustic || !passes(t.name)) continue;
                entry(t.name, t.words);
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(listStart.x, y)); ImGui::Dummy(ImVec2(1, 1));
        ImGui::EndChild(); ImGui::PopStyleVar();
    }

    void analyseWav(HWND window) {
        wchar_t name[MAX_PATH] = L"";
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;
        dialog.lpstrFilter = L"Wave (*.wav)\0*.wav\0";
        dialog.lpstrFile = name; dialog.nMaxFile = MAX_PATH; dialog.lpstrDefExt = L"wav";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&dialog)) return;
        std::string reason;
        const auto source = headspace::sourceFromAudio(name, reason);
        if (!source) { status = reason; statusAlarm = true; return; }
        catalog.entries.push_back(*source);
        sourceTab = 0;
        aim(source->name, source->words);
    }

    bool slider(ImVec2 start, float width, float& value) {
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddText(start, ink, "AMOUNT");
        char text[32]; std::snprintf(text, sizeof(text), "%.3f", value);
        osd(draw, ImVec2(start.x + width - osdSize(text).x, start.y), accent, text);
        const ImVec2 rail(start.x + 8, start.y + 39);
        hit("amount", ImVec2(start.x, start.y + 22), ImVec2(width, 36));
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
        controlAudition |= ImGui::IsItemActive() || (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0);
        const float before = value;
        if (ImGui::IsItemActive()) value = std::clamp((ImGui::GetIO().MousePos.x - rail.x) / (width - 16), 0.f, 1.f);
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0)
            value = std::clamp(value + ImGui::GetIO().MouseWheel * .001f, 0.f, 1.f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Packed-word interpolation from the frozen corner to the anchor\nWheel for 0.001 steps");
        const float px = rail.x + value * (width - 16);
        draw->AddLine(rail, ImVec2(rail.x + width - 16, rail.y), grid, 1);
        draw->AddLine(rail, ImVec2(px, rail.y), accent, 1);
        draw->AddCircleFilled(ImVec2(px, rail.y), 10, ground);
        draw->AddCircleFilled(ImVec2(px, rail.y), 5, accent);
        return value != before;
    }

    void macroDeck(ImVec2 start, float width) {
        auto* draw = ImGui::GetWindowDrawList();
        const char* labels[]{"FREQ", "OCTAVES", "TENSION"};
        float* values[]{&macroScale, &macroSpan, &macroTension};
        const float low[]{-12.f, .25f, 0.f}, high[]{12.f, 2.5f, 1.f};
        for (int index = 0; index < 3; ++index) {
            const ImVec2 at(start.x, start.y + 22 + index * 32);
            char text[40]; std::snprintf(text, sizeof(text), "%s  %+.3f", labels[index], *values[index]);
            osd(draw, at, muted, text);
            const ImVec2 rail(at.x + 8, at.y + 22);
            hit(("macro " + std::to_string(index)).c_str(), ImVec2(at.x, at.y + 10), ImVec2(width, 22));
            controlAudition |= ImGui::IsItemActive();
            const float before = *values[index];
            if (ImGui::IsItemActive())
                *values[index] = std::clamp((ImGui::GetIO().MousePos.x - rail.x) / (width - 16)
                    * (high[index] - low[index]) + low[index], low[index], high[index]);
            const float px = rail.x + (*values[index] - low[index]) / (high[index] - low[index]) * (width - 16);
            draw->AddLine(rail, ImVec2(rail.x + width - 16, rail.y), grid, 1);
            draw->AddCircleFilled(ImVec2(px, rail.y), 4, accent);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s over all six sections\nNeutral: scale 0, stress 1, tension 0.5, tilt 0", labels[index]);
            if (*values[index] != before) applyMacros();
        }
    }

    void exportBody(HWND window) {
        wchar_t name[MAX_PATH] = L"headspace.body240";
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;
        dialog.lpstrFilter = L"TRENCH body (*.body240)\0*.body240\0";
        dialog.lpstrFile = name; dialog.nMaxFile = MAX_PATH; dialog.lpstrDefExt = L"body240";
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetSaveFileNameW(&dialog)) {
            try { model.exportBody(name); status = "EXPORTED 240 BYTES"; statusAlarm = false; }
            catch (const std::exception& e) { status = e.what(); statusAlarm = true; }
        }
    }

    void frame(HWND window) {
        historyAction = false; controlAudition = false;
        std::uint32_t message;
        bool midiChanged = false;
        while (midi.pop(message)) midiChanged |= performance.message(message);
        if (midiChanged && performance.active) {
            morph = performance.morph; q = performance.q;
            candidate.freeze(model.body(morph, q)); rebase();
        }
        if (analyseRequested) { analyseRequested = false; analyseWav(window); }
        const auto before = captureEdit();
        if (ImGui::IsMouseClicked(0)) gesture = before;
        if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::GetIO().KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) travelHistory(ImGui::GetIO().KeyShift);
            else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) travelHistory(true);
        }
        const auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos); ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("HEADSPACE", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();
        const auto origin = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        auto* draw = ImGui::GetWindowDrawList();
        preview = nullptr;
        draw->AddText(ImGui::GetFont(), 28, ImVec2(origin.x + 22, origin.y + 14), ink, "HEADSPACE");
        draw->AddRectFilled(ImVec2(origin.x, origin.y + 47), ImVec2(origin.x + 80, origin.y + 50), accent);
        if (!initialized) {
            ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 70));
            bootstrap(); ImGui::End(); return;
        }
        const bool bench = silentTest;
        if (bench) {
            if (chip("undo", "UNDO", ImVec2(origin.x + width - 370, origin.y), ImVec2(65, 32))) travelHistory(false);
            if (chip("redo", "REDO", ImVec2(origin.x + width - 302, origin.y), ImVec2(65, 32))) travelHistory(true);
            if (chip("templates", "NEW BODY", ImVec2(origin.x + width - 224, origin.y), ImVec2(100, 32))) ImGui::OpenPopup("Initialize endpoints");
            if (ImGui::BeginPopup("Initialize endpoints")) { bootstrap(); ImGui::EndPopup(); }
            if (chip("export", "EXPORT BODY", ImVec2(origin.x + width - 112, origin.y), ImVec2(112, 32))) exportBody(window);
        }
        const float contentBottom = viewport->WorkSize.y - 128;
        const float bay1 = width * .60f, bay2 = width * .25f, bay3 = width * .15f;
        const ImVec2 bodyStart(origin.x, origin.y);
        const float zRow = std::min(126.f, bay1 / 6.2f);
        const ImVec2 bodySize(bay1, viewport->WorkSize.y - 100 - origin.y - zRow - 14);
        const float column = bay2 - 44;
        const ImVec2 edgeStart(origin.x + bay1 + 22, origin.y + 112);
        const ImVec2 sourceStart(origin.x + bay1 + bay2 + 22, origin.y + 112);
        const float sourceWidth = bay3 - 44;
        const auto strikes = performance.strikes + manualStrikes;
        if (strikes > seenStrikes) { strikeFlash = 1.f; strikeVelocity = performance.active ? performance.velocity : 1.f; }
        seenStrikes = strikes;
        strikeFlash = std::max(0.f, strikeFlash - ImGui::GetIO().DeltaTime * 6.f);
        membrane(bodyStart, bodySize);
        for (std::size_t z = 0; z < 6; ++z)
            stagePlot(ImVec2(bodyStart.x + 6 + z * (zRow + 4), bodyStart.y + bodySize.y + 8), zRow, z);
        {
        card(ImVec2(edgeStart.x - 12, edgeStart.y - 14), ImVec2(column + 4, 290), "MORPH");
        card(ImVec2(edgeStart.x - 12, edgeStart.y + 286), ImVec2(column + 4, 184), "MACROS");
        char label[128];
        std::snprintf(label, sizeof(label), "BASE    %s", frozenLabel().c_str());
        draw->AddText(ImVec2(edgeStart.x, edgeStart.y + 18), frozen, label);
        std::snprintf(label, sizeof(label), "TARGET  %s", candidate.aimed() ? anchorName.c_str() : "-");
        draw->AddText(ImVec2(edgeStart.x, edgeStart.y + 44), candidate.aimed() ? temporary : muted, label);
        if (chip("stamp candidate", "STAMP  M100 Q0", ImVec2(edgeStart.x, edgeStart.y + 76), ImVec2(column - 20, 30))) stamp(1);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy the audible candidate into M100/Q0. M0/Q0 and the other corners stay untouched.");
        const float pairWidth = (column - 36) / 3;
        if (chip("pair natural", "NATURAL", ImVec2(edgeStart.x, edgeStart.y + 112), ImVec2(pairWidth, 26),
                candidate.pairing() == headspace::Pairing::kNatural))
            candidate.setPairing(headspace::Pairing::kNatural);
        if (chip("pair cross", "CROSS F1xF2", ImVec2(edgeStart.x + pairWidth + 8, edgeStart.y + 112), ImVec2(pairWidth, 26),
                candidate.pairing() == headspace::Pairing::kCross))
            candidate.setPairing(headspace::Pairing::kCross);
        if (chip("pair invert", "INVERT ALL", ImVec2(edgeStart.x + 2 * (pairWidth + 8), edgeStart.y + 112), ImVec2(pairWidth, 26),
                candidate.pairing() == headspace::Pairing::kInvert))
            candidate.setPairing(headspace::Pairing::kInvert);
        if (chip("lock poles", "LOCK POLES", ImVec2(edgeStart.x, edgeStart.y + 146), ImVec2(pairWidth, 26), polesLocked)) polesLocked = !polesLocked;
        if (chip("lock zeros", "LOCK ZEROS", ImVec2(edgeStart.x + pairWidth + 8, edgeStart.y + 146), ImVec2(pairWidth, 26), zerosLocked)) zerosLocked = !zerosLocked;
        float amount = candidate.amount();
        if (slider(ImVec2(edgeStart.x, edgeStart.y + 186), column - 20, amount)) {
            candidate.setAmount(amount);
            aimTarget = amount;
            status.clear(); statusAlarm = false;
        }
        if (candidate.edited()) {
            draw->AddText(ImVec2(edgeStart.x, edgeStart.y + 254), warn, "EDITED");
            if (chip("clear edit", "CLEAR", ImVec2(edgeStart.x + 90, edgeStart.y + 248), ImVec2(80, 24))) {
                candidate.detach(); candidate.setAmount(0);
            }
        }
        macroDeck(ImVec2(edgeStart.x, edgeStart.y + 308), column - 20);
        sources(sourceStart, ImVec2(sourceWidth, contentBottom - sourceStart.y));
        }

        const ImVec2 footer(origin.x + 22, viewport->WorkSize.y - 92);
        draw->AddLine(ImVec2(footer.x, footer.y - 12), ImVec2(footer.x + width, footer.y - 12), grid);
        if (chip("play", strikeMode ? "STRIKE" : playing ? "Stop" : (preview || controlAudition) ? "HEARING" : "PLAY", footer, ImVec2(66, 32), playing) && audioReady) {
            if (strikeMode) ++manualStrikes;
            else playing = !playing;
        }
        char sourceLabel[64];
        std::snprintf(sourceLabel, sizeof(sourceLabel), performance.active ? "MIDI / %.0f HZ" : "SAW / %.0f HZ", performance.active ? performance.hz() : 110.);
        if (strikeMode) std::snprintf(sourceLabel, sizeof(sourceLabel), "IMPULSE / BODY");
        draw->AddText(ImVec2(footer.x + 82, footer.y + 8), muted, sourceLabel);
        hit("performance input", ImVec2(footer.x + 82, footer.y), ImVec2(165, 30));
        if (ImGui::IsItemClicked()) { strikeMode = !strikeMode; playing = false; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nClick to switch SAW / IMPULSE.\nChannel 1: note = saw pitch or impulse strike; velocity = level + Q; CC1 = Morph.\nImpulse pitch belongs to the filter body. Note-off leaves its tail ringing.\nShift+1..4 recalls (and strikes in IMPULSE); plain 1..4 captures.\nLive capture copies the last rendered block's words. Escape releases MIDI.\nConnect MIDI before launching.", midi.name.empty() ? "No MIDI input available" : midi.name.c_str());
        const ImVec2 gainStart(footer.x + 260, footer.y + 15);
        hit("monitor level", ImVec2(gainStart.x, gainStart.y - 14), ImVec2(180, 28));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Monitor gain after a -18 dBFS saw reference; -6 dB gives -24 dBFS RMS. DC-referenced plot and packed export stay independent of this fader.");
        if (ImGui::IsItemActive()) outputDb = std::clamp((ImGui::GetIO().MousePos.x - gainStart.x) / 180 * 36 - 36, -36.f, 0.f);
        draw->AddLine(gainStart, ImVec2(gainStart.x + 180, gainStart.y), grid, 1);
        draw->AddCircleFilled(ImVec2(gainStart.x + (outputDb + 36) / 36 * 180, gainStart.y), 5, accent);
        char value[96]; std::snprintf(value, sizeof(value), "LISTEN  %+.0f dB", outputDb);
        osd(draw, ImVec2(gainStart.x + 200, footer.y + 9), muted, value);
        if (chip("level mode", matchedLevel ? "MATCH" : "TRUE",
                ImVec2(footer.x + 860, footer.y), ImVec2(66, 30), !matchedLevel)) matchedLevel = !matchedLevel;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("MATCH normalises every state to -18 dBFS for comparing anchors. TRUE lets the filter's own level through, up to 34 dB of range across the bank.");
        for (int o = 0; o < kOverlayCount; ++o)
            if (chip(kOverlayName[o], kOverlayName[o], ImVec2(footer.x + 560 + o * 58, footer.y), ImVec2(54, 30), overlay == o))
                overlay = o;
        if (!audioReady) draw->AddText(ImVec2(footer.x + width - 200, footer.y + 8), muted, "Audio unavailable");
        else {
            const double peakDb = 20 * std::log10(std::max(audio.peak(), 1e-9f));
            std::snprintf(value, sizeof(value), "PEAK %.1f dBFS", peakDb);
            osd(draw, ImVec2(footer.x + width - 130, footer.y + 9),
                peakDb >= -3 ? alarm : peakDb >= -12 ? warn : peakDb >= -60 ? safe : muted, value);
        }
        if (!status.empty()) draw->AddText(ImVec2(footer.x, footer.y + 43), statusAlarm ? alarm : accent, status.c_str());
        if (candidate.aimed() && candidate.amount() < aimTarget) {
            const float step = ImGui::GetIO().DeltaTime / .25f;
            candidate.setAmount(std::min(aimTarget, candidate.amount() + step));
            controlAudition = true;
        }
        membraneGrab(bodyStart, bodySize);
        if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { performance.release(); performance.active = false; }
            for (int c = 0; c < 4; ++c) {
                if (!ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + c), false)) continue;
                const auto corner = static_cast<std::size_t>(c);
                if (ImGui::GetIO().KeyShift) {
                    landed(corner);
                    status = "CORNER " + coordinate(corner); statusAlarm = false;
                    controlAudition = true;
                    if (strikeMode) ++manualStrikes;
                } else stamp(corner);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) stamp(1);
        }
        auto endpoint = preview ? *preview : candidate.words();
        if (performance.active && performance.gate) {
            std::array<double, 6> voiced{};
            const auto held = performance.voices(voiced);
            const double quality = 3. + 22. * std::clamp(performance.velocity, 0.f, 1.f);
            if (held >= 2 || (overlay > 0 && held == 1)) {
                std::array<headspace::Pole, 6> chord{};
                bool usable = true;
                for (std::size_t s = 0; s < 6; ++s) {
                    const double hz = overlay > 0 && held == 1
                        ? voiced[0] * std::exp2(kOverlaySteps[overlay][s] / 12.)
                        : voiced[s % held] * std::exp2(static_cast<double>(s / held));
                    if (!(hz > 20) || hz > 18000) { usable = false; break; }
                    chord[s] = {hz, std::max(hz / quality, kNarrowestVoiceHz)};
                }
                if (usable) try { endpoint = headspace::compile(chord); }
                    catch (const std::exception&) {}
            } else {
                try {
                    endpoint = headspace::shape(endpoint,
                        std::clamp(performance.note - kKeyCentre, -12, 12),
                        .5 + .5 * performance.velocity, 1., 0.);
                } catch (const std::exception&) {}
            }
        }
        const auto resolved = headspace::resolve(endpoint, rate);
        if (levelWords != endpoint || levelMatched != matchedLevel) {
            levelReference = matchedLevel ? headspace::listeningGain(resolved, -18) : 1.0;
            levelWords = endpoint; levelMatched = matchedLevel;
        }
        if (audio.publish({resolved, !silentTest && (performance.active ? performance.gate : (playing || preview != nullptr || controlAudition)), static_cast<float>(levelReference * std::pow(10., outputDb / 20)), 110., performance.active ? performance.velocity : 1.f, strikeMode, silentTest ? 0. : performance.strikes + manualStrikes})) displayed = resolved;
        if (!audioReady) audio.process({});
        ImGui::SetCursorScreenPos(ImVec2(origin.x, footer.y + 70)); ImGui::Dummy(ImVec2(width, 1));
        ImGui::End();
        if (!historyAction) {
            if (gesture) {
                if (!ImGui::IsMouseDown(0)) { history.commit(*gesture, captureEdit()); gesture.reset(); }
            } else history.commit(before, captureEdit());
        }
    }
};

}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) return true;
    if (message == WM_SIZE && wParam != SIZE_MINIMIZED) {
        resizeWidth = LOWORD(lParam); resizeHeight = HIWORD(lParam); return 0;
    }
    if (message == WM_GETMINMAXINFO) {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = {1000, 740}; return 0;
    }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command, int show) {
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, windowProc, 0, 0, instance, nullptr, nullptr, nullptr, nullptr, L"HEADSPACE", nullptr};
    RegisterClassExW(&wc);
    const bool compact = std::wstring(command).find(L"--compact") != std::wstring::npos;
    const auto window = CreateWindowW(wc.lpszClassName, L"HEADSPACE", WS_OVERLAPPEDWINDOW, 80, 70, compact ? 1000 : 1320, compact ? 740 : 900,
        nullptr, nullptr, instance, nullptr);
    if (!createDevice(window)) { cleanup(); DestroyWindow(window); return 1; }
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    for (auto& color : style.Colors) color.x = color.y = color.z = (color.x + color.y + color.z) / 3;
    style.WindowPadding = ImVec2(22, 18); style.ItemSpacing = ImVec2(10, 8); style.FramePadding = ImVec2(8, 5);
    style.FrameRounding = 0; style.WindowRounding = 0; style.PopupRounding = 0; style.GrabRounding = 0;
    style.ScrollbarRounding = 0; style.PopupBorderSize = 1; style.FrameBorderSize = 1;
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0, 0, 0, 1);
    style.Colors[ImGuiCol_Border] = ImVec4(.16f, .17f, .18f, 1);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 1);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0, 0, 0, 1);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0, 1, .52f, 1);
    style.Colors[ImGuiCol_SliderGrab] = style.Colors[ImGuiCol_CheckMark];
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(.98f, .96f, .90f, 1);
    static const ImWchar glyphs[]{0x20, 0xff, 0x250, 0x2af, 0};
    ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 16, nullptr, glyphs);
    osdMono = ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 14);
    ImGui_ImplWin32_Init(window); ImGui_ImplDX11_Init(device, context);
    ShowWindow(window, show); UpdateWindow(window);
    int result = 0;
    const bool smoke = std::wstring(command).find(L"--smoke") != std::wstring::npos;
    try {
        App app;
        if (smoke) { app.silentTest = true; app.initialized = true; app.model.bootstrap(app.library.entries[0], &app.library.entries[1]); app.freeze(); }
        const auto initialModel = app.model;
        auto afterFirstStamp = app.model, afterSecondStamp = app.model;
        headspace::Endpoint beforeWords, afterAmount, beforeStamp;
        bool done = false; int frameCount = 0;
        while (!done) {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
                if (message.message == WM_QUIT) done = true;
            }
            if (done) break;
            if (IsIconic(window)) { Sleep(10); continue; }
            if (resizeWidth && resizeHeight) {
                if (renderTarget) { renderTarget->Release(); renderTarget = nullptr; }
                swapChain->ResizeBuffers(0, resizeWidth, resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
                resizeWidth = resizeHeight = 0; createRenderTarget();
            }
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame();
            if (smoke) {
                auto& io = ImGui::GetIO();
                io.AddFocusEvent(true);
                const float width = io.DisplaySize.x - 44;
                const ImVec2 edgeStart(width - 340 - 24, 112);
                const ImVec2 sourceStart(24, 112);
                const float listTop = sourceStart.y + 52;
                const auto mouse = [&](float x, float y, bool down) { io.AddMousePosEvent(x, y); io.AddMouseButtonEvent(0, down); };
                if (frameCount < 2 || frameCount == 26) io.AddMousePosEvent(-1000, -1000);
                if (frameCount == 2) mouse(sourceStart.x + 110, listTop + 118, true);
                if (frameCount == 3) io.AddMouseButtonEvent(0, false);
                if (frameCount == 4) mouse(sourceStart.x + 40, listTop + 170, true);
                if (frameCount == 5) io.AddMouseButtonEvent(0, false);
                if (frameCount == 8) mouse(edgeStart.x + 60, edgeStart.y + 225, true);
                if (frameCount == 9) io.AddMousePosEvent(edgeStart.x + 60 + 200, edgeStart.y + 225);
                if (frameCount == 10) io.AddMouseButtonEvent(0, false);
                if (frameCount == 12 || frameCount == 13) mouse(edgeStart.x + 80, 225, frameCount == 12);
                if (frameCount == 14) beforeStamp = app.candidate.words();
                if (frameCount == 15 || frameCount == 16) io.AddKeyEvent(ImGuiKey_3, frameCount == 15);
                if (frameCount == 18) mouse(edgeStart.x + 40, edgeStart.y + 350, true);
                if (frameCount == 19) io.AddMousePosEvent(edgeStart.x + 150, edgeStart.y + 350);
                if (frameCount == 20) io.AddMouseButtonEvent(0, false);
                if (frameCount >= 23 && frameCount <= 27) {
                    io.AddKeyEvent(ImGuiMod_Ctrl, frameCount <= 24);
                    io.AddKeyEvent(ImGuiKey_Z, frameCount <= 25);
                }
                if (frameCount >= 28 && frameCount <= 32) {
                    io.AddKeyEvent(ImGuiMod_Ctrl, frameCount <= 29);
                    io.AddKeyEvent(ImGuiKey_Z, frameCount <= 30);
                }
                if (frameCount == 32) io.AddMousePosEvent(-1000, -1000);
                if (frameCount >= 34 && frameCount <= 36) {
                    io.AddKeyEvent(ImGuiMod_Shift, frameCount <= 35);
                    io.AddKeyEvent(ImGuiKey_1, frameCount == 34);
                }
                if (frameCount == 37) mouse(130, io.DisplaySize.y - 77, true);
                if (frameCount == 38) io.AddMouseButtonEvent(0, false);
                if (frameCount == 39) mouse(50, io.DisplaySize.y - 77, true);
                if (frameCount == 40) io.AddMouseButtonEvent(0, false);
            }
            ImGui::NewFrame();
            app.frame(window);
            if (smoke) {
                if (frameCount == 2 && (app.model != initialModel || app.candidate.aimed() || app.candidate.amount() != 0))
                    throw std::runtime_error("Freeze wrote the model or aimed");
                if (frameCount == 6 && (!app.candidate.aimed() || app.model != initialModel
                        || app.candidate.destination() == app.candidate.original()))
                    throw std::runtime_error("Anchor selection did not aim a different body or wrote the model");
                if (frameCount == 7) beforeWords = app.candidate.words();
                if (frameCount == 11) {
                    if (app.candidate.words() == beforeWords || app.model != initialModel)
                        throw std::runtime_error("Amount did not move the candidate or wrote the model");
                    afterAmount = app.candidate.words();
                }
                if (frameCount == 14) {
                    if (app.model.corners()[1] != afterAmount) throw std::runtime_error("Stamp did not copy the audible candidate");
                    if (app.model.corners()[0] != initialModel.corners()[0]) throw std::runtime_error("Stamp modified M0/Q0");
                    afterFirstStamp = app.model;
                }
                if (frameCount == 17 && (app.model.corners()[2] != beforeStamp || app.candidate.words() != beforeStamp))
                    throw std::runtime_error("Key 3 did not stamp the audible candidate");
                if (frameCount == 17) afterSecondStamp = app.model;
                if (frameCount == 21 && (app.candidate.words() == afterAmount || app.model != afterSecondStamp))
                    throw std::runtime_error("Macro did not shape the candidate or wrote the model");
                if (frameCount == 26 && (app.candidate.words() != afterAmount || app.macroScale != 0))
                    throw std::runtime_error("Undo did not restore the macro and the candidate");
                if (frameCount == 33 && (app.model != afterFirstStamp || app.candidate.words() != afterAmount))
                    throw std::runtime_error("Undo did not restore the corner and the candidate");
                if (frameCount == 36 && (app.morph != 0 || app.q != 0 || app.model.bytes() != afterFirstStamp.bytes()
                        || app.model.names() != afterFirstStamp.names()
                        || app.candidate.aimed() || app.candidate.words() != initialModel.corners()[0]))
                    throw std::runtime_error("Shift+1 did not jump the puck to corner 0 without writing");
                if (frameCount == 41 && (!app.strikeMode || app.manualStrikes != 1
                        || app.model.bytes() != afterFirstStamp.bytes()))
                    throw std::runtime_error("Impulse selector or manual strike failed or changed saved corners");
            }
            ImGui::Render();
            constexpr float clear[]{0.1f, 0.1f, 0.1f, 1};
            context->OMSetRenderTargets(1, &renderTarget, nullptr);
            context->ClearRenderTargetView(renderTarget, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if (smoke && frameCount == 1) capture("headspace-canvas.bmp");
            if (smoke && frameCount == 6) capture("headspace-anchor.bmp");
            if (smoke && frameCount == 11) capture("headspace-amount.bmp");
            if (smoke && frameCount == 18) capture("headspace-macro.bmp");
            swapChain->Present(1, 0);
            if (smoke && ++frameCount == 43) done = true;
        }
    } catch (const std::exception& e) {
        if (smoke) { std::ofstream log("headspace-smoke-error.txt"); log << e.what(); }
        else MessageBoxA(window, e.what(), "HEADSPACE", MB_OK | MB_ICONERROR);
        result = 1;
    }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    cleanup(); DestroyWindow(window); UnregisterClassW(wc.lpszClassName, instance);
    return result;
}
