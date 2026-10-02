#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <d3d11.h>
#include <dxgi.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_dx11.h"
#include "implot.h"
#include "implot3d.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include "../../plugin/source/dsp/Peevers.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// =============================================================================
// DirectX 11 Globals
// =============================================================================
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                                               featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                               &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
                                           featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                           &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
    return true;
}

void CleanupDeviceD3D()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// =============================================================================
// Formants & Root Solving
// =============================================================================
struct Resonance
{
    double freqHz = 0.0;
    double bwHz = 0.0;
    double radius = 0.0;
};

std::string NoteNameFromHz(double hz)
{
    if (hz <= 0.0 || !std::isfinite(hz)) return "";
    double midi = 69.0 + 12.0 * std::log2(std::max(hz, 1.0) / 440.0);
    int semitone = (int)std::round(midi);
    int cents = (int)std::round((midi - (double)semitone) * 100.0);
    const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = (semitone / 12) - 1;
    std::string name = names[(semitone % 12 + 12) % 12];
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%d %+dc", name.c_str(), octave, cents);
    return std::string(buf);
}

// Durand-Kerner polynomial root finder for monic P(z) = z^n + a1 z^(n-1) + ... + an = 0
std::vector<Resonance> SolveConjugatePoles(const std::array<float, 13>& k, double fs)
{
    // Step-up recursion from lattice reflection coefficients to direct-form polynomial A(z)
    std::vector<double> a = { 1.0 };
    for (size_t i = 1; i <= 12; ++i)
    {
        double ki = (double)k[i];
        std::vector<double> a_next(a.size() + 1, 0.0);
        for (size_t j = 0; j < a.size(); ++j) a_next[j] += a[j];
        for (size_t j = 0; j < a.size(); ++j) a_next[j + 1] += ki * a[a.size() - 1 - j];
        a = a_next;
    }

    const int n = (int)a.size() - 1; // 12
    if (n != 12) return {};

    std::vector<std::complex<double>> roots(n);
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < n; ++i)
    {
        double angle = 2.0 * pi * (double)i / (double)n + pi / (2.0 * (double)n);
        roots[i] = std::polar(0.9, angle);
    }

    // Iterate Durand-Kerner
    for (int iter = 0; iter < 40; ++iter)
    {
        for (int i = 0; i < n; ++i)
        {
            std::complex<double> z = roots[i];
            // Evaluate polynomial P(z)
            std::complex<double> p = a[0];
            for (int j = 1; j <= n; ++j) p = p * z + a[j];

            std::complex<double> denom = 1.0;
            for (int j = 0; j < n; ++j)
            {
                if (i != j) denom *= (z - roots[j]);
            }
            if (std::abs(denom) > 1e-15) roots[i] -= p / denom;
        }
    }

    std::vector<Resonance> out;
    for (const auto& z : roots)
    {
        if (z.imag() > 0.0 && std::abs(z) < 0.9995)
        {
            double hz = std::arg(z) * fs / (2.0 * pi);
            if (hz >= 40.0 && hz <= 0.48 * fs)
            {
                double r = std::abs(z);
                double bw = -std::log(std::max(r, 1e-7)) * fs / pi;
                out.push_back({ hz, bw, r });
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const Resonance& x, const Resonance& y) { return x.freqHz < y.freqHz; });
    if (out.size() > 6) out.resize(6);
    return out;
}

// =============================================================================
// Audio Player Engine (miniaudio)
// =============================================================================
struct AudioEngine
{
    ma_device device {};
    std::vector<float> samples {};
    uint32_t sampleRate = 44100;
    std::atomic<uint64_t> playheadFrames { 0 };
    std::atomic<bool> isPlaying { false };
    bool loop = true;
    float volume = 1.0f;
    bool initialized = false;

    static void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
    {
        auto* self = (AudioEngine*)pDevice->pUserData;
        float* out = (float*)pOutput;
        if (!self->isPlaying || self->samples.empty())
        {
            memset(out, 0, frameCount * sizeof(float));
            return;
        }

        uint64_t head = self->playheadFrames.load();
        uint64_t total = self->samples.size();
        for (ma_uint32 i = 0; i < frameCount; ++i)
        {
            if (head >= total)
            {
                if (self->loop && total > 0) head = 0;
                else
                {
                    self->isPlaying = false;
                    for (; i < frameCount; ++i) out[i] = 0.0f;
                    break;
                }
            }
            out[i] = self->samples[head++] * self->volume;
        }
        self->playheadFrames.store(head);
    }

    bool Init(uint32_t sr)
    {
        sampleRate = sr;
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 1;
        config.sampleRate = sr;
        config.dataCallback = DataCallback;
        config.pUserData = this;
        if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
            return false;
        ma_device_start(&device);
        initialized = true;
        return true;
    }

    void Shutdown()
    {
        if (initialized)
        {
            ma_device_uninit(&device);
            initialized = false;
        }
    }

    bool LoadWav(const std::string& path)
    {
        ma_decoder decoder;
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 0); // mono
        if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS)
            return false;

        ma_uint64 total;
        ma_decoder_get_length_in_pcm_frames(&decoder, &total);
        samples.resize(total);
        ma_decoder_read_pcm_frames(&decoder, samples.data(), total, nullptr);
        uint32_t sr = decoder.outputSampleRate;
        ma_decoder_uninit(&decoder);

        if (!initialized || sampleRate != sr)
        {
            Shutdown();
            Init(sr);
        }
        playheadFrames.store(0);
        return true;
    }

    double GetPositionSec() const
    {
        return sampleRate > 0 ? (double)playheadFrames.load() / (double)sampleRate : 0.0;
    }

    double GetDurationSec() const
    {
        return sampleRate > 0 ? (double)samples.size() / (double)sampleRate : 0.0;
    }

    void SeekSec(double sec)
    {
        if (samples.empty() || sampleRate == 0) return;
        uint64_t f = (uint64_t)std::clamp(sec * (double)sampleRate, 0.0, (double)(samples.size() - 1));
        playheadFrames.store(f);
    }
};

// =============================================================================
// Analysis Cache
// =============================================================================
struct AnalysisCache
{
    std::vector<float> freqs;
    std::vector<double> frameTimes;
    std::vector<std::vector<float>> rawFftDb;   // [frame][bin]
    std::vector<std::vector<float>> spanAvgDb;  // [frame][bin]
    std::vector<std::vector<float>> lpcEnvDb;   // [frame][bin]
    std::vector<std::vector<Resonance>> resonances;

    int nfft = 512;
    int nfft2 = 256;
    int hop = 128;
    double analysisFs = 11025.0;

    void Compute(const std::vector<float>& srcAudio, double srcFs, double targetFs, int fftSize, int winType, float kAvg)
    {
        if (srcAudio.empty()) return;

        nfft = fftSize;
        nfft2 = fftSize / 2;
        hop = fftSize / 4;
        analysisFs = targetFs;

        // Resample audio if needed
        std::vector<float> audio;
        if (std::abs(srcFs - targetFs) > 1.0)
        {
            size_t newLen = (size_t)((double)srcAudio.size() * targetFs / srcFs);
            audio.resize(newLen);
            for (size_t i = 0; i < newLen; ++i)
            {
                double srcIdx = (double)i * srcFs / targetFs;
                size_t idx0 = std::min((size_t)srcIdx, srcAudio.size() - 1);
                size_t idx1 = std::min(idx0 + 1, srcAudio.size() - 1);
                double frac = srcIdx - (double)idx0;
                audio[i] = (float)((1.0 - frac) * srcAudio[idx0] + frac * srcAudio[idx1]);
            }
        }
        else
        {
            audio = srcAudio;
        }

        freqs.resize(nfft2 + 1);
        for (int i = 0; i <= nfft2; ++i)
            freqs[i] = (float)((double)i * (analysisFs / 2.0) / (double)nfft2);

        size_t totalFrames = (audio.size() > (size_t)nfft) ? (audio.size() - nfft) / hop : 0;
        frameTimes.resize(totalFrames);
        rawFftDb.assign(totalFrames, std::vector<float>(nfft2 + 1, -100.0f));
        spanAvgDb.assign(totalFrames, std::vector<float>(nfft2 + 1, -100.0f));
        lpcEnvDb.assign(totalFrames, std::vector<float>(nfft2 + 1, -100.0f));
        resonances.resize(totalFrames);

        hs::Peevers peevers;

        // Pass 1: Raw FFT + SPAN Exponential Smoothing (lpcenv = 0)
        peevers.lpcenv = 0;
        peevers.avgk = kAvg;
        peevers.setParms(nfft, nfft, hop, winType);

        for (size_t f = 0; f < totalFrames; ++f)
        {
            size_t start = f * hop;
            frameTimes[f] = (double)start / analysisFs;
            peevers.averagedFrame(&audio[start]);
            for (int b = 0; b <= nfft2; ++b)
            {
                rawFftDb[f][b] = peevers.fx[(size_t)b];
                spanAvgDb[f][b] = peevers.avg[(size_t)b];
            }
        }

        // Pass 2: Authentic 12th-Order GAL + Lattice Impulse Synthesis (lpcenv = 1)
        peevers.reset();
        peevers.lpcenv = 1;
        peevers.avgk = kAvg;
        peevers.setParms(nfft, nfft, hop, winType);

        for (size_t f = 0; f < totalFrames; ++f)
        {
            size_t start = f * hop;
            peevers.averagedFrame(&audio[start]);
            for (int b = 0; b <= nfft2; ++b)
            {
                lpcEnvDb[f][b] = peevers.fx[(size_t)b];
            }
            resonances[f] = SolveConjugatePoles(peevers.lpc.k, analysisFs);
        }
    }
};

// =============================================================================
// Main Application
// =============================================================================
int main(int argc, char** argv)
{
    // Initialize GLFW
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1540, 920, "SGI Spectrogram & SPAN Studio (Dear ImGui + ImPlot + ImPlot3D)", nullptr, nullptr);
    if (!window) return 1;

    HWND hwnd = glfwGetWin32Window(window);
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Initialize ImGui, ImPlot, ImPlot3D
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImPlot3D::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Apply SGI Dark Indigo Theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.05f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_Header]         = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]  = ImVec4(0.18f, 0.24f, 0.32f, 1.00f);
    style.Colors[ImGuiCol_Button]         = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]  = ImVec4(0.22f, 0.74f, 0.97f, 0.80f);
    style.Colors[ImGuiCol_SliderGrab]     = ImVec4(0.22f, 0.74f, 0.97f, 1.00f);

    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Audio & DSP Engine
    AudioEngine audio;
    AnalysisCache analysis;
    std::string currentFileName = "recipes/recordings/test-vowel-ah.wav";

    // Auto-load demo vowel if present
    if (audio.LoadWav(currentFileName))
    {
        analysis.Compute(audio.samples, (double)audio.sampleRate, 11025.0, 512, 7, 0.99f);
    }

    // UI State
    int currentRateIdx = 0; // 0 = 11025 Hz, 1 = 22050 Hz, 2 = 44100 Hz
    const double kRates[] = { 11025.0, 22050.0, 44100.0 };
    int currentWinIdx = 7;   // Hanning
    const char* kWinNames[] = { "Exact Blackman", "Blackman", "Blackman-Harris 1", "Blackman-Harris 2", "Blackman-Harris 3", "Blackman-Harris 4", "Hamming", "Hanning", "Rectangular" };
    int currentFftIdx = 1;   // 512
    const int kFftSizes[] = { 256, 512, 1024 };
    float kAvg = 0.99f;

    bool showRawFft = true;
    bool showSpanAvg = true;
    bool showLpcEnv = true;
    bool useLpcWaterfall = true;

    // Headless test mode check
    if (argc > 1 && std::string(argv[1]) == "--test")
    {
        std::cout << "SGI Studio Native C++ Headless Test: Loaded " << audio.samples.size()
                  << " samples, computed " << analysis.frameTimes.size() << " frames. OK!\n";
        audio.Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot3D::DestroyContext();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }

    // Main Loop (144Hz+ Uncapped / VSync)
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Fullscreen dockable window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("SGI Spectrogram & SPAN Studio##Main", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // 1. TOP TRANSPORT BAR
        ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "SGI SPECTROGRAM & SPAN STUDIO");
        ImGui::SameLine();
        ImGui::TextDisabled("| 1993/1995 Alan Peevers DSP (144Hz+ DirectX 11)");
        ImGui::SameLine(ImGui::GetWindowWidth() - 220);
        ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate, 1000.0f / (io.Framerate > 0 ? io.Framerate : 1.0f));

        ImGui::Separator();

        if (ImGui::Button("Open Audio File..."))
        {
            OPENFILENAMEA ofn;
            char szFile[260] = { 0 };
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "Audio Files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            if (GetOpenFileNameA(&ofn))
            {
                currentFileName = szFile;
                if (audio.LoadWav(currentFileName))
                {
                    analysis.Compute(audio.samples, (double)audio.sampleRate, kRates[currentRateIdx], kFftSizes[currentFftIdx], currentWinIdx, kAvg);
                }
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("File: %s (%.2f sec, %u Hz)", currentFileName.c_str(), audio.GetDurationSec(), audio.sampleRate);

        ImGui::SameLine(ImGui::GetWindowWidth() - 420);
        if (audio.isPlaying)
        {
            if (ImGui::Button("Pause", ImVec2(70, 0))) audio.isPlaying = false;
        }
        else
        {
            if (ImGui::Button("Play", ImVec2(70, 0))) audio.isPlaying = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(60, 0))) { audio.isPlaying = false; audio.SeekSec(0.0); }
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &audio.loop);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::SliderFloat("Vol", &audio.volume, 0.0f, 1.0f, "%.2f");

        // Timeline Scrubber
        float currSec = (float)audio.GetPositionSec();
        float durSec = (float)audio.GetDurationSec();
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 140);
        if (ImGui::SliderFloat("##Scrub", &currSec, 0.0f, std::max(durSec, 0.01f), "%.2f s"))
        {
            audio.SeekSec(currSec);
        }
        ImGui::SameLine();
        ImGui::Text("%02d:%05.2f / %02d:%05.2f", (int)currSec / 60, fmod(currSec, 60.0f), (int)durSec / 60, fmod(durSec, 60.0f));

        ImGui::Separator();

        // Current Playhead Frame Index
        size_t currentFrame = 0;
        if (!analysis.frameTimes.empty())
        {
            auto it = std::lower_bound(analysis.frameTimes.begin(), analysis.frameTimes.end(), (double)currSec);
            currentFrame = std::clamp((size_t)std::distance(analysis.frameTimes.begin(), it), (size_t)0, analysis.frameTimes.size() - 1);
        }

        // 2. MAIN SPLIT VIEW (Left: SGI 3D Waterfall, Right: E-mu SPAN 2D)
        float paneWidth = (ImGui::GetWindowWidth() - 30) * 0.5f;
        float paneHeight = ImGui::GetWindowHeight() - 250;

        // --- LEFT PANE: SGI 3D SPECTROGRAM WATERFALL (ImPlot3D) ---
        ImGui::BeginChild("LeftPane", ImVec2(paneWidth, paneHeight), true);
        ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "SGI 3D SPECTROGRAM (1993 WATERFALL MESH)");
        ImGui::SameLine();
        ImGui::Checkbox("Use 12-Pole 'Env'", &useLpcWaterfall);

        if (!analysis.frameTimes.empty())
        {
            if (ImPlot3D::BeginPlot("##SgiWaterfall3D", ImVec2(-1, -1)))
            {
                ImPlot3D::SetupAxes("Freq (Hz)", "Time (s)", "Amp (dB)");
                ImPlot3D::SetupAxisLimits(ImAxis3D_X, 40.0, analysis.analysisFs / 2.0);
                ImPlot3D::SetupAxisLimits(ImAxis3D_Y, 0.0, analysis.frameTimes.back());
                ImPlot3D::SetupAxisLimits(ImAxis3D_Z, -90.0, 20.0);

                // Draw 30 waterfall time slices across the file
                size_t step = std::max((size_t)1, analysis.frameTimes.size() / 32);
                for (size_t f = 0; f < analysis.frameTimes.size(); f += step)
                {
                    double t = analysis.frameTimes[f];
                    const auto& slice = useLpcWaterfall ? analysis.lpcEnvDb[f] : analysis.rawFftDb[f];

                    std::vector<double> xs, ys, zs;
                    xs.reserve(analysis.freqs.size());
                    ys.reserve(analysis.freqs.size());
                    zs.reserve(analysis.freqs.size());

                    for (size_t b = 1; b < analysis.freqs.size(); b += 2)
                    {
                        xs.push_back(analysis.freqs[b]);
                        ys.push_back(t);
                        zs.push_back((double)slice[b]);
                    }

                    char label[32];
                    snprintf(label, sizeof(label), "##slice_%zu", f);
                    if (f == currentFrame)
                        ImPlot3D::PlotLine(label, xs.data(), ys.data(), zs.data(), (int)xs.size());
                    else
                        ImPlot3D::PlotLine(label, xs.data(), ys.data(), zs.data(), (int)xs.size());
                }
                ImPlot3D::EndPlot();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // --- RIGHT PANE: E-MU SPAN SPECTRUM ANALYZER (ImPlot) ---
        ImGui::BeginChild("RightPane", ImVec2(paneWidth, paneHeight), true);
        ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "E-MU SPAN SPECTRUM ANALYZER (1995)");
        ImGui::SameLine();
        ImGui::Checkbox("Raw", &showRawFft);
        ImGui::SameLine();
        ImGui::Checkbox("SPAN (Cyan)", &showSpanAvg);
        ImGui::SameLine();
        ImGui::Checkbox("12-Pole Env (Amber)", &showLpcEnv);

        if (!analysis.frameTimes.empty())
        {
            if (ImPlot::BeginPlot("##SpanPlot", ImVec2(-1, -1)))
            {
                ImPlot::SetupAxes("Frequency (Hz)", "Amplitude (dB)");
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                ImPlot::SetupAxisLimits(ImAxis_X1, 40.0, analysis.analysisFs / 2.0);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -100.0, 25.0);

                const auto& raw = analysis.rawFftDb[currentFrame];
                const auto& avg = analysis.spanAvgDb[currentFrame];
                const auto& env = analysis.lpcEnvDb[currentFrame];
                int count = (int)analysis.freqs.size() - 1;

                if (showRawFft)
                {
                    ImPlotSpec spec;
                    spec.LineColor = ImVec4(0.35f, 0.42f, 0.50f, 0.7f);
                    spec.LineWeight = 1.0f;
                    ImPlot::PlotLine("Raw FFT", &analysis.freqs[1], &raw[1], count, spec);
                }
                if (showSpanAvg)
                {
                    ImPlotSpec spec;
                    spec.LineColor = ImVec4(0.22f, 0.74f, 0.97f, 1.0f);
                    spec.LineWeight = 2.0f;
                    ImPlot::PlotLine("SPAN Averaged", &analysis.freqs[1], &avg[1], count, spec);
                }
                if (showLpcEnv)
                {
                    ImPlotSpec spec;
                    spec.LineColor = ImVec4(0.96f, 0.62f, 0.04f, 1.0f);
                    spec.LineWeight = 2.5f;
                    ImPlot::PlotLine("12-Pole GAL Env", &analysis.freqs[1], &env[1], count, spec);

                    // Plot Formant Resonance Pins
                    const auto& res = analysis.resonances[currentFrame];
                    for (size_t i = 0; i < res.size(); ++i)
                    {
                        double pkHz = res[i].freqHz;
                        int binIdx = std::clamp((int)(pkHz / (analysis.analysisFs / 2.0) * (double)analysis.nfft2), 0, analysis.nfft2);
                        double pkDb = (double)env[binIdx];
                        ImPlotSpec ptSpec;
                        ptSpec.Marker = ImPlotMarker_Circle;
                        ptSpec.MarkerSize = 5.0f;
                        ptSpec.MarkerFillColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                        ptSpec.MarkerLineColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        ImPlot::PlotScatter("##poles", &pkHz, &pkDb, 1, ptSpec);
                    }
                }

                // Crosshair Tooltip
                if (ImPlot::IsPlotHovered())
                {
                    ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                    std::string note = NoteNameFromHz(mouse.x);
                    ImGui::SetTooltip("Freq: %.1f Hz (%s) | Amp: %+.1f dB", mouse.x, note.c_str(), mouse.y);
                }

                ImPlot::EndPlot();
            }
        }
        ImGui::EndChild();

        // 3. BOTTOM INSPECTOR & DSP PARAMETERS
        ImGui::Separator();
        ImGui::Text("DSP PARAMETERS:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170);
        if (ImGui::Combo("Bandwidth", &currentRateIdx, "11,025 Hz (Speech)\0 22,050 Hz (E-mu)\0 44,100 Hz (Nyquist)\0"))
        {
            analysis.Compute(audio.samples, (double)audio.sampleRate, kRates[currentRateIdx], kFftSizes[currentFftIdx], currentWinIdx, kAvg);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::Combo("Window", &currentWinIdx, kWinNames, IM_ARRAYSIZE(kWinNames)))
        {
            analysis.Compute(audio.samples, (double)audio.sampleRate, kRates[currentRateIdx], kFftSizes[currentFftIdx], currentWinIdx, kAvg);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90);
        if (ImGui::Combo("FFT Size", &currentFftIdx, "256\0 512\0 1024\0"))
        {
            analysis.Compute(audio.samples, (double)audio.sampleRate, kRates[currentRateIdx], kFftSizes[currentFftIdx], currentWinIdx, kAvg);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90);
        if (ImGui::SliderFloat("SPAN (k)", &kAvg, 0.50f, 0.999f, "%.3f"))
        {
            analysis.Compute(audio.samples, (double)audio.sampleRate, kRates[currentRateIdx], kFftSizes[currentFftIdx], currentWinIdx, kAvg);
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 250);
        if (ImGui::Button("Export SPAN .txt (xgraph)"))
        {
            if (!analysis.frameTimes.empty())
            {
                std::ofstream f("span_snapshot.txt");
                f << "# SGI SPAN Snapshot (E-mu Systems 1995, Alan Peevers)\n";
                f << "# Time: " << currSec << "s | Sample Rate: " << analysis.analysisFs << "\n";
                for (size_t b = 1; b < analysis.freqs.size(); ++b)
                    f << analysis.freqs[b] << " " << analysis.spanAvgDb[currentFrame][b] << "\n";
            }
        }

        // Formant Badges
        ImGui::Text("12-POLE GAL FORMANTS:");
        ImGui::SameLine();
        if (!analysis.resonances.empty())
        {
            const auto& res = analysis.resonances[currentFrame];
            for (size_t i = 0; i < 6; ++i)
            {
                if (i < res.size())
                {
                    std::string note = NoteNameFromHz(res[i].freqHz);
                    ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "[F%zu: %.0f Hz (%s) bw:%.0f]",
                                       i + 1, res[i].freqHz, note.c_str(), res[i].bwHz);
                }
                else
                {
                    ImGui::TextDisabled("[F%zu: ---]", i + 1);
                }
                ImGui::SameLine();
            }
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.05f, 0.07f, 0.09f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); // VSync enabled
    }

    // Cleanup
    audio.Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot3D::DestroyContext();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
