#include "Body.h"

namespace ws
{
bool Body::ready() const
{
    for (int c : corner) if (c < 0) return false;
    return true;
}

Words Body::cornerWords (const std::vector<Frame>& frames, int i) const
{
    Words w = frames[(size_t) corner[(size_t) i]].words;
    if (i > 0 && corner[0] >= 0 && corner[0] != corner[(size_t) i]) w = compile (leadTo (frames[(size_t) corner[0]].chord, frames[(size_t) corner[(size_t) i]].chord).b, kDatumHz);
    else if (i == 0)
    {
        for (int k = 1; k < 4; ++k)
            if (corner[(size_t) k] >= 0 && corner[(size_t) k] != corner[0]) { w = compile (leadTo (frames[(size_t) corner[0]].chord, frames[(size_t) corner[(size_t) k]].chord).a, kDatumHz); break; }
    }
    for (int s = 0; s < kRows; ++s)
        if (! rowOn[(size_t) s])
            for (int k = 0; k < kWords; ++k) w[(size_t) s][(size_t) k] = trench::core::kIdentitySection[(size_t) k];
    if (unity) unityDc (w);
    return w;
}

std::array<double, 4> Body::weights() const
{
    return { (1.0 - morph) * (1.0 - q), morph * (1.0 - q), (1.0 - morph) * q, morph * q };
}

Morph Body::wheelMorph (const std::vector<Frame>& frames) const
{
    std::array<Words, 4> cw;
    for (int i = 0; i < 4; ++i) cw[(size_t) i] = cornerWords (frames, i);
    return ws::wheelMorph (cw, morph, q);
}

Words Body::wheelWords (const std::vector<Frame>& frames) const { return wheelMorph (frames).words; }

bool Body::outside() const { return morph < 0.0 || morph > 1.0 || q < 0.0 || q > 1.0; }

std::array<std::uint8_t, trench::core::kLegacyBodyBytes> Body::legacyBytes (const std::vector<Frame>& frames) const
{
    trench::core::PackedBody body;
    for (auto& c : body.words) c.fill (trench::core::kIdentitySection);
    for (int i = 0; i < 4; ++i)
    {
        const auto w = cornerWords (frames, i);
        for (int s = 0; s < kRows; ++s)
            for (int k = 0; k < kWords; ++k)
                body.words[(size_t) i][(size_t) s][(size_t) k] = w[(size_t) s][(size_t) k];
    }
    for (int i = 4; i < 8; ++i) body.words[(size_t) i] = body.words[(size_t) (i - 4)];
    return body.legacy_bytes();
}

bool Body::exportTo (const std::vector<Frame>& frames, const juce::File& file) const
{
    if (! ready()) return false;
    const auto bytes = legacyBytes (frames);
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    return file.replaceWithData (bytes.data(), bytes.size());
}
}
