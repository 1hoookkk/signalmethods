#include "LensProcessor.h"
#include "LensEditor.h"
#include <cmath>

namespace lens
{
namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout out;
    out.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "x", 1 }, "X", 0.0f, 1.0f, 0.5f));
    out.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "y", 1 }, "Y", 0.0f, 1.0f, 0.5f));
    out.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return out;
}

juce::String titleOf (const juce::String& stem)
{
    juce::String out;
    for (const auto& word : juce::StringArray::fromTokens (stem.replaceCharacter ('_', ' '), " ", ""))
        out += (out.isEmpty() ? "" : " ") + word.substring (0, 1).toUpperCase() + word.substring (1);
    return out;
}
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "LENS", layout())
{
    x = state.getRawParameterValue ("x");
    y = state.getRawParameterValue ("y");
    bypass = state.getRawParameterValue ("bypass");
    loadLibrary();
    const char* const wanted[3] = { "Ubu Orator M0 Q1", "Talking Hedz M1 Q1", "Dead Ringer M0 Q0" };
    const float spots[3][2] = { { 0.25f, 0.25f }, { 0.72f, 0.42f }, { 0.78f, 0.84f } };
    for (int i = 0; i < 3; ++i)
    {
        Snapshot s;
        bool found = false;
        for (const auto& l : library) if (l.name == wanted[i]) { s = l; found = true; break; }
        if (! found && library.size() > (size_t) i) s = library[(size_t) i];
        if (! found && library.empty()) { s.name = "identity"; s.words.fill (trench::core::kIdentitySection); }
        s.x = spots[i][0]; s.y = spots[i][1];
        placed.push_back (s);
    }
}

void Processor::loadLibrary()
{
    const juce::File dir = juce::File (TRENCH_TABLE_STITCH_ROOT).getChildFile ("plugin/presets/p2k");
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.body240");
    files.sort();
    const char* const corners[4] = { "M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1" };
    for (const auto& file : files)
    {
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb) || mb.getSize() != trench::core::kLegacyBodyBytes) continue;
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> ((const std::uint8_t*) mb.getData(), mb.getSize()));
        for (size_t c = 0; c < 4; ++c)
        {
            Snapshot s;
            s.name = titleOf (file.getFileNameWithoutExtension()) + " " + corners[c];
            for (size_t row = 0; row < trench::core::kSectionCount; ++row) s.words[row] = body.words[c][row];
            library.push_back (s);
        }
    }
}

void Processor::placeNext (size_t dot)
{
    std::lock_guard<std::mutex> lock (placedLock);
    if (dot >= placed.size() || library.empty()) return;
    size_t at = 0;
    for (size_t i = 0; i < library.size(); ++i) if (library[i].name == placed[dot].name) { at = (i + 1) % library.size(); break; }
    const float px = placed[dot].x, py = placed[dot].y;
    placed[dot] = library[at];
    placed[dot].x = px; placed[dot].y = py;
    lastX = -1.0f;
}

void Processor::moveDot (size_t dot, float nx, float ny)
{
    std::lock_guard<std::mutex> lock (placedLock);
    if (dot >= placed.size()) return;
    placed[dot].x = juce::jlimit (0.0f, 1.0f, nx);
    placed[dot].y = juce::jlimit (0.0f, 1.0f, ny);
    lastX = -1.0f;
}

trench::core::CornerWords Processor::blend (float px, float py) const
{
    trench::core::CornerWords out {};
    out.fill (trench::core::kIdentitySection);
    if (placed.empty()) return out;
    std::vector<double> weights (placed.size(), 0.0);
    double total = 0.0;
    for (size_t i = 0; i < placed.size(); ++i)
    {
        const double d2 = (double) ((px - placed[i].x) * (px - placed[i].x) + (py - placed[i].y) * (py - placed[i].y));
        if (d2 < 1.0e-6) { std::fill (weights.begin(), weights.end(), 0.0); weights[i] = 1.0; total = 1.0; break; }
        weights[i] = 1.0 / (d2 * d2);
        total += weights[i];
    }
    for (size_t row = 0; row < trench::core::kSectionCount; ++row)
        for (size_t w = 0; w < 5; ++w)
        {
            double acc = 0.0;
            for (size_t i = 0; i < placed.size(); ++i) acc += weights[i] / total * (double) placed[i].words[row][w];
            out[row][w] = (std::uint16_t) std::clamp (std::lround (acc), 0L, 65535L);
        }
    return out;
}

void Processor::setPuck (float nx, float ny)
{
    if (auto* p = state.getParameter ("x")) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, nx));
    if (auto* p = state.getParameter ("y")) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, ny));
}

void Processor::clearPath()
{
    std::lock_guard<std::mutex> lock (pathLock);
    path.clear();
    playIndex = 0; playhead = 0.0; clock = 0.0;
}

void Processor::prepareToPlay (double sampleRate, int)
{
    rate = sampleRate;
    for (auto& r : runners)
    {
        r.set_sample_rate (rate);
        r.reset();
        r.set_ring_leveller (true);
        r.set_pole_distortion (0.0);
        r.set_immediate (trench::core::native::rewarp_cascade (blend (x->load(), y->load()), trench::core::kP2kDatumHz, rate));
    }
    lastX = x->load(); lastY = y->load();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const double blockSeconds = (double) n / rate;
    float px = x->load(), py = y->load();
    if (playing.load())
    {
        std::lock_guard<std::mutex> lock (pathLock);
        if (path.size() > 1)
        {
            const double length = path.back().t;
            playhead = length > 0.0 ? std::fmod (playhead + blockSeconds, length) : 0.0;
            while (playIndex + 1 < path.size() && path[playIndex + 1].t < playhead) ++playIndex;
            if (path[playIndex].t > playhead) playIndex = 0;
            const auto& a = path[playIndex];
            const auto& b = path[std::min (playIndex + 1, path.size() - 1)];
            const double span = std::max (1.0e-6, b.t - a.t), f = std::clamp ((playhead - a.t) / span, 0.0, 1.0);
            px = (float) (a.x + (b.x - a.x) * f); py = (float) (a.y + (b.y - a.y) * f);
            x->store (px); y->store (py);
        }
    }
    else if (recording.load())
    {
        std::lock_guard<std::mutex> lock (pathLock);
        clock += blockSeconds;
        if (path.empty() || std::abs (path.back().x - px) > 1.0e-4f || std::abs (path.back().y - py) > 1.0e-4f || clock - path.back().t > 0.05)
            path.push_back ({ clock, px, py });
    }
    if (px != lastX || py != lastY)
    {
        std::unique_lock<std::mutex> lock (placedLock, std::try_to_lock);
        if (lock.owns_lock())
        {
            const auto cascade = trench::core::native::rewarp_cascade (blend (px, py), trench::core::kP2kDatumHz, rate);
            for (auto& r : runners) r.set_glide (cascade, 64);
            lastX = px; lastY = py;
        }
    }
    if (bypass->load() > 0.5f) return;
    for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
        runners[(size_t) ch].process (std::span<float> (buffer.getWritePointer (ch), (size_t) n));
}

void Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    {
        std::lock_guard<std::mutex> lock (placedLock);
        juce::ValueTree dots ("dots");
        for (const auto& s : placed)
        {
            juce::ValueTree d ("dot");
            d.setProperty ("name", s.name, nullptr); d.setProperty ("x", s.x, nullptr); d.setProperty ("y", s.y, nullptr);
            dots.appendChild (d, nullptr);
        }
        tree.appendChild (dots, nullptr);
    }
    {
        std::lock_guard<std::mutex> lock (pathLock);
        juce::MemoryBlock steps;
        for (const auto& p : path) { steps.append (&p.t, sizeof (double)); steps.append (&p.x, sizeof (float)); steps.append (&p.y, sizeof (float)); }
        tree.setProperty ("path", steps.toBase64Encoding(), nullptr);
    }
    if (auto xml = tree.createXml()) copyXmlToBinary (*xml, destData);
}

void Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return;
    {
        std::lock_guard<std::mutex> lock (placedLock);
        const auto dots = tree.getChildWithName ("dots");
        for (int i = 0; i < dots.getNumChildren() && i < (int) placed.size(); ++i)
        {
            const auto d = dots.getChild (i);
            for (const auto& l : library) if (l.name == d["name"].toString()) { placed[(size_t) i] = l; break; }
            placed[(size_t) i].x = (float) d["x"]; placed[(size_t) i].y = (float) d["y"];
        }
        lastX = -1.0f;
    }
    {
        std::lock_guard<std::mutex> lock (pathLock);
        juce::MemoryBlock steps;
        steps.fromBase64Encoding (tree["path"].toString());
        path.clear();
        const size_t stride = sizeof (double) + 2 * sizeof (float);
        for (size_t at = 0; at + stride <= steps.getSize(); at += stride)
        {
            Step s;
            std::memcpy (&s.t, (const char*) steps.getData() + at, sizeof (double));
            std::memcpy (&s.x, (const char*) steps.getData() + at + sizeof (double), sizeof (float));
            std::memcpy (&s.y, (const char*) steps.getData() + at + sizeof (double) + sizeof (float), sizeof (float));
            path.push_back (s);
        }
    }
    tree.removeChild (tree.getChildWithName ("dots"), nullptr);
    tree.removeProperty ("path", nullptr);
    state.replaceState (tree);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new lens::Processor(); }
