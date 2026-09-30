#pragma once
#include <RTNeural/RTNeural.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
namespace trench
{
class EightBusDesk
{
public:
    static constexpr int kHidden = 32;
    static constexpr double kModelRate = 48000.0;
    static constexpr float kInputCeiling = 0.9f;
    static constexpr double kBlockerHz = 5.0;
    static constexpr int kWarmSamples = 256;
    using Model = RTNeural::ModelT<float, 1, 1, RTNeural::LSTMLayerT<float, 1, kHidden>, RTNeural::DenseT<float, kHidden, 1>>;

    EightBusDesk() : model (std::make_unique<Model>()) {}

    bool load (const void* jsonData, size_t jsonSize, float calibrateAmplitude = 0.2f, float driveScale = 1.0f)
    {
        loaded = false;
        if (jsonData == nullptr || jsonSize == 0)
            return false;
        try
        {
            const std::string text (static_cast<const char*> (jsonData), jsonSize);
            const auto json = nlohmann::json::parse (text);
            const auto& layers = json.at ("layers");
            if (layers.size() != 2 || layers[0].at ("shape").back().get<int>() != kHidden)
                return false;
            model->parseJson (json, false);
            probeAmplitude = calibrateAmplitude;
            inScale = driveScale;
            loaded = true;
            calibrate();
            return true;
        }
        catch (...)
        {
            loaded = false;
            return false;
        }
    }
    bool isLoaded() const noexcept { return loaded; }
    void prepare (double hostRate, int maxBlock)
    {
        rate = hostRate > 0.0 ? hostRate : kModelRate;
        resampling = std::abs (rate - kModelRate) > 0.5;
        chunk = std::max (maxBlock, 1);
        inFifo.assign ((size_t) chunk + 32, 0.0f);
        midFifo.assign ((size_t) (2 * (int) std::ceil ((double) chunk * kModelRate / rate) + 64), 0.0f);
        blockerCoefficient = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * kBlockerHz / kModelRate);
        reset();
    }
    void reset() noexcept
    {
        model->reset();
        toModel.reset();
        fromModel.reset();
        inCount = 0;
        midCount = 0;
        blockerIn = 0.0f;
        blockerOut = 0.0f;
        if (loaded)
            for (int i = 0; i < kWarmSamples; ++i)
                forward (0.0f);
    }
    float smallSignalGainDb() const noexcept { return gainDb; }

    void process (float* data, int numSamples) noexcept
    {
        if (! loaded || numSamples <= 0 || (resampling && inFifo.empty()))
            return;
        if (! resampling)
        {
            for (int i = 0; i < numSamples; ++i)
                data[i] = forward (data[i]);
            return;
        }
        while (numSamples > chunk)
        {
            processChunk (data, chunk);
            data += chunk;
            numSamples -= chunk;
        }
        processChunk (data, numSamples);
    }

private:
    void processChunk (float* data, int numSamples) noexcept
    {
        std::copy (data, data + numSamples, inFifo.begin() + inCount);
        inCount += numSamples;
        const double up = rate / kModelRate;
        const int produce = std::clamp ((int) std::floor ((double) (inCount - 1) / up), 0, (int) midFifo.size() - midCount);
        const int used = toModel.process (up, inFifo.data(), midFifo.data() + midCount, produce);
        for (int i = midCount; i < midCount + produce; ++i)
            midFifo[(size_t) i] = forward (midFifo[(size_t) i]);
        midCount += produce;
        std::move (inFifo.begin() + used, inFifo.begin() + inCount, inFifo.begin());
        inCount -= used;

        const double down = kModelRate / rate;
        const int need = (int) std::floor (1.0 + (double) numSamples * down) + 1;
        if (midCount < need)
        {
            const int deficit = std::min (need - midCount, (int) midFifo.size() - midCount);
            std::move_backward (midFifo.begin(), midFifo.begin() + midCount, midFifo.begin() + midCount + deficit);
            std::fill (midFifo.begin(), midFifo.begin() + deficit, 0.0f);
            midCount += deficit;
        }
        const int usedMid = fromModel.process (down, midFifo.data(), data, numSamples);
        std::move (midFifo.begin() + usedMid, midFifo.begin() + midCount, midFifo.begin());
        midCount -= usedMid;
    }
    float forward (float x) noexcept
    {
        const float driven = std::clamp (x * inScale, -kInputCeiling, kInputCeiling);
        const float raw = model->forward (&driven);
        const float blocked = raw - blockerIn + blockerCoefficient * blockerOut;
        blockerIn = raw;
        blockerOut = blocked;
        const float y = blocked * outPad;
        return std::isfinite (y) ? y : 0.0f;
    }
    void calibrate()
    {
        constexpr int warm = 12000, measure = 9600;
        model->reset();
        double inPower = 0.0, outPower = 0.0;
        for (int i = 0; i < warm + measure; ++i)
        {
            const float x = probeAmplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (double) i / kModelRate);
            const float y = model->forward (&x);
            if (i >= warm)
            {
                inPower += (double) x * x;
                outPower += (double) y * y;
            }
        }
        const double gain = inPower > 0.0 && outPower > 0.0 ? std::sqrt (outPower / inPower) : 1.0;
        outPad = (float) (1.0 / (gain * (double) inScale));
        gainDb = (float) (20.0 * std::log10 (gain));
        model->reset();
    }
    std::unique_ptr<Model> model;
    juce::Interpolators::Lagrange toModel, fromModel;
    std::vector<float> inFifo, midFifo;
    int inCount = 0;
    int midCount = 0;
    int chunk = 1;
    double rate = kModelRate;
    bool resampling = false;
    bool loaded = false;
    float inScale = 1.0f;
    float outPad = 1.0f;
    float gainDb = 0.0f;
    float probeAmplitude = 0.2f;
    float blockerCoefficient = 1.0f;
    float blockerIn = 0.0f;
    float blockerOut = 0.0f;
};
}
