#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace trench
{
class QSoundStage final
{
public:
    static constexpr double kNativeRate = 11025.0;
    static constexpr double kTailSeconds = 23.0 / kNativeRate;

    void prepare (double sampleRate)
    {
        const double ratio = sampleRate / kNativeRate;
        const auto count = (size_t) std::ceil ((double) (kShadow.size() - 1) * ratio) + 1;
        taps.resize (count);
        history.assign (count, {});
        for (size_t i = 0; i < count; ++i)
        {
            const double position = std::min ((double) i / ratio, (double) (kShadow.size() - 1));
            const auto lower = (size_t) position;
            const auto upper = std::min (lower + 1, kShadow.size() - 1);
            const double fraction = position - (double) lower;
            taps[i] = (float) ((kShadow[lower] + fraction * (kShadow[upper] - kShadow[lower])) / 16384.0 / ratio);
        }
        write = 0;
        amount.reset (sampleRate, 0.020);
        amount.setCurrentAndTargetValue (0.0f);
    }

    void setEnabled (bool enabled) noexcept
    {
        amount.setTargetValue (enabled ? 1.0f : 0.0f);
    }

    void process (float& left, float& right) noexcept
    {
        if (history.empty()) return;
        const float dryLeft = left, dryRight = right;
        history[write] = { std::isfinite (left) ? left : 0.0f, std::isfinite (right) ? right : 0.0f };
        const float mix = amount.getNextValue();
        if (mix > 0.0f)
        {
            float shadowLeft = 0.0f, shadowRight = 0.0f;
            auto read = write;
            for (const auto tap : taps)
            {
                shadowLeft += tap * history[read][0];
                shadowRight += tap * history[read][1];
                read = read == 0 ? history.size() - 1 : read - 1;
            }
            left = dryLeft + mix * (kNearGain * dryLeft + shadowRight - dryLeft);
            right = dryRight + mix * (kNearGain * dryRight + shadowLeft - dryRight);
        }
        if (++write == history.size()) write = 0;
    }

private:
    static constexpr float kNearGain = 16382.0f / 16384.0f;
    static constexpr std::array<std::int16_t, 23> kShadow {
        0, -9616, -3474, -1891, -356, 1151, 937, 672, 282, -51, -156, -160,
        -100, -31, 10, 27, 25, 14, 3, -2, -4, -3, -2
    };
    std::vector<float> taps;
    std::vector<std::array<float, 2>> history;
    size_t write = 0;
    juce::SmoothedValue<float> amount;
};
}
