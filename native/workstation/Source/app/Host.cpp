#include "Host.h"
#include "Widgets.h"

#include <d3d11.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <fstream>
#include <stdexcept>

namespace {

ID3D11Device* device{};
ID3D11DeviceContext* context{};
IDXGISwapChain* swapChain{};
ID3D11RenderTargetView* renderTarget{};
UINT resizeWidth{}, resizeHeight{};

void createRenderTarget() {
    ID3D11Texture2D* buffer{};
    if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) {
        device->CreateRenderTargetView(buffer, nullptr, &renderTarget);
        buffer->Release();
    }
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

}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT WINAPI windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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

namespace headspace {

bool Host::init(HINSTANCE instance, int show, bool compact) {
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, windowProc, 0, 0, instance, nullptr, nullptr, nullptr, nullptr, L"HEADSPACE", nullptr};
    RegisterClassExW(&wc);
    window_ = CreateWindowW(wc.lpszClassName, L"HEADSPACE", WS_OVERLAPPEDWINDOW, 80, 70,
        compact ? 1000 : 1320, compact ? 740 : 900, nullptr, nullptr, instance, nullptr);
    if (!createDevice(window_)) { shutdown(); DestroyWindow(window_); return false; }
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
    ui::osdMono = ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 14);
    ImGui_ImplWin32_Init(window_); ImGui_ImplDX11_Init(device, context);
    ShowWindow(window_, show); UpdateWindow(window_);
    return true;
}

bool Host::running() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message); DispatchMessageW(&message);
        if (message.message == WM_QUIT) return false;
    }
    if (IsIconic(window_)) { Sleep(10); return running(); }
    if (resizeWidth && resizeHeight) {
        if (renderTarget) { renderTarget->Release(); renderTarget = nullptr; }
        swapChain->ResizeBuffers(0, resizeWidth, resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        resizeWidth = resizeHeight = 0; createRenderTarget();
    }
    return true;
}

void Host::beginFrame() {
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame();
}

void Host::endFrame() {
    ImGui::Render();
    const float clear[4]{0, 0, 0, 1};
    context->OMSetRenderTargets(1, &renderTarget, nullptr);
    context->ClearRenderTargetView(renderTarget, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swapChain->Present(1, 0);
}

void Host::shutdown() {
    if (renderTarget) { renderTarget->Release(); renderTarget = nullptr; }
    if (swapChain) { swapChain->Release(); swapChain = nullptr; }
    if (context) { context->Release(); context = nullptr; }
    if (device) { device->Release(); device = nullptr; }
}

void Host::capture(const char* filename) const {
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

}
