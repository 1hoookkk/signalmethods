#pragma once
#include "PluginProcessor.h"
#include "BinaryData.h"
#include <trench/core/packed_body.hpp>
#include <array>
#include <cmath>
#include <cstdio>

inline int morphSmoothingTests()
{
    int failures = 0;
    const auto check = [&] (bool ok, const char* name)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", name);
        if (! ok) ++failures;
    };
    const auto set = [] (PluginProcessor& p, const char* id, float value)
    {
        if (auto* parameter = p.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (const bool qAxis : { false, true })
        {
            juce::MemoryBlock body (BinaryData::identity_body240, (size_t) BinaryData::identity_body240Size);
            auto* bytes = static_cast<unsigned char*> (body.getData());
            for (int corner = 0; corner < 4; ++corner)
            {
                const bool high = (corner & (qAxis ? 2 : 1)) != 0;
                const auto word = trench::core::encode_word (high ? 0.1875 : 0.125);
                const int offset = (corner * 30 + 4) * 2;
                bytes[offset] = (unsigned char) (word & 255);
                bytes[offset + 1] = (unsigned char) (word >> 8);
            }
            const auto render = [&] (int blockSize)
            {
                PluginProcessor p;
                p.setRateAndBufferSizeDetails (rate, blockSize);
                p.prepareToPlay (rate, blockSize);
                check (p.installBodyBytes (body.getData(), body.getSize(), rate), "smoothing gain fixture loads");
                for (const auto* id : { ParamID::morph, ParamID::q, ParamID::movePreset,
                                       ParamID::preamp, ParamID::output })
                    set (p, id, 0.0f);
                juce::AudioBuffer<float> audio (2, blockSize);
                juce::MidiBuffer midi;
                const auto process = [&]
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < blockSize; ++i)
                            audio.setSample (c, i, 0.125f);
                    p.processBlock (audio, midi);
                };
                for (int i = 0; i < 1024; i += blockSize) process();
                std::array<float, 4096> heard {};
                for (int direction = 0; direction < 2; ++direction)
                {
                    set (p, qAxis ? ParamID::q : ParamID::morph, direction == 0 ? 1.0f : 0.0f);
                    for (int start = 0; start < 2048; start += blockSize)
                    {
                        process();
                        for (int i = 0; i < blockSize; ++i)
                            heard[(size_t) (direction * 2048 + start + i)] = audio.getSample (0, i);
                    }
                }
                return heard;
            };
            const auto small = render (64);
            const auto large = render (512);
            double worst = 0.0;
            bool finite = true;
            for (size_t i = 0; i < small.size(); ++i)
            {
                finite = finite && std::isfinite (small[i]) && std::isfinite (large[i]);
                worst = std::max (worst, std::abs ((double) small[i] - large[i]));
            }
            const auto progress = [&] (int sample, bool falling)
            {
                const double position = ((double) large[(size_t) sample] / 0.125 - 0.5) / 0.25;
                return falling ? 1.0 - position : position;
            };
            for (const bool falling : { false, true })
            {
                const int offset = falling ? 2048 : 0;
                const double first = progress (offset, falling);
                const double oneMs = progress (offset + (int) std::ceil (rate * 0.001) - 1, falling);
                const double sixMs = progress (offset + (int) std::ceil (rate * 0.006) - 1, falling);
                std::printf ("  %s %.0f Hz %s: first %.6f, 1 ms %.6f, 6 ms %.6f, block error %.3e\n",
                             qAxis ? "Q" : "MORPH", rate, falling ? "fall" : "rise", first, oneMs, sixMs, worst);
                check (first > 0.0 && first < 0.12, "wheel step retains a short transition instead of jumping instantly");
                check (oneMs > 0.47 && oneMs < 0.51 && sixMs > 0.98 && sixMs <= 1.0001,
                       "wheel settles promptly with only the subtle smoothing stage");
            }
            check (finite && worst < 1.0e-7, "MORPH/Q audio is independent of 64 versus 512 sample host blocks");
        }
    return failures;
}
