#pragma once
#include "FuncGenPatterns.h"
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

namespace trench
{
struct UserMotion
{
    std::array<float, 64> values { -0.5f, -0.425f, -0.2f, 0.175f, 0.5f, 0.175f, -0.2f, -0.425f };
    int steps = 8, direction = 0, length = 3, playback = 1;
    int loopSteps = 0;
    bool smooth = true, dirty = true;
    double stepBeats = 0.5, rateHz = 0.0;
    juce::String name { "My movement" };

    FuncGenPattern pattern() const noexcept
    {
        return { nullptr, steps, direction, smooth, values.data(), nullptr, stepBeats, rateHz };
    }
    static UserMotion copyFactory (int index, int transition, int lengthChoice, int playbackChoice)
    {
        UserMotion out;
        if (index < 1 || index > kNumFuncGenPatterns) return out;
        const auto& p = kFuncGenPatterns[index - 1];
        out.name = p.name;
        out.steps = p.steps;
        out.smooth = transition == 2 || (transition == 0 && p.smooth);
        out.stepBeats = p.stepBeats;
        out.rateHz = p.rateHz;
        out.loopSteps = p.direction == 2 ? 2 * p.steps - 2 : p.steps;
        out.length = lengthChoice;
        out.playback = playbackChoice;
        const bool once = playbackChoice == 2 || (playbackChoice == 0 && p.direction == 5);
        out.direction = once ? 5 : 0;
        for (int i = 0; i < p.steps; ++i)
            out.values[(size_t) i] = p.values[p.direction == 1 ? p.steps - 1 - i : i];
        if (p.direction == 2)
        {
            for (int i = p.steps - 2; i > 0 && out.steps < 63; --i)
                out.values[(size_t) out.steps++] = p.values[i];
            if (once) out.values[(size_t) out.steps++] = p.values[0];
        }
        return out;
    }
    juce::var toJson() const
    {
        if (steps < 2 || steps > (int) values.size()) return {};
        auto* object = new juce::DynamicObject();
        juce::var result (object);
        object->setProperty ("version", 1);
        object->setProperty ("name", name);
        object->setProperty ("smooth", smooth);
        object->setProperty ("direction", direction);
        object->setProperty ("length", length);
        object->setProperty ("playback", playback);
        object->setProperty ("stepBeats", stepBeats);
        object->setProperty ("rateHz", rateHz);
        object->setProperty ("loopSteps", loopSteps);
        object->setProperty ("dirty", dirty);
        juce::Array<juce::var> points;
        for (int i = 0; i < steps; ++i) points.add (values[(size_t) i]);
        object->setProperty ("points", points);
        return result;
    }
    static bool fromJson (const juce::var& value, UserMotion& out)
    {
        const auto* o = value.getDynamicObject();
        if (o == nullptr || (int) o->getProperty ("version") != 1) return false;
        auto* points = o->getProperty ("points").getArray();
        if (points == nullptr || points->size() < 2 || points->size() > 64) return false;
        UserMotion candidate;
        candidate.name = o->getProperty ("name").toString().trim();
        if (candidate.name.isEmpty() || candidate.name.length() > 32) return false;
        candidate.steps = points->size();
        for (int i = 0; i < candidate.steps; ++i)
        {
            const auto& v = points->getReference (i);
            if (! v.isDouble() && ! v.isInt() && ! v.isInt64()) return false;
            const double n = (double) v;
            if (! std::isfinite (n) || n < -1.0 || n > 1.0) return false;
            candidate.values[(size_t) i] = (float) n;
        }
        candidate.direction = (int) o->getProperty ("direction");
        candidate.smooth = (bool) o->getProperty ("smooth");
        candidate.dirty = (bool) o->getProperty ("dirty");
        candidate.length = (int) o->getProperty ("length");
        candidate.playback = (int) o->getProperty ("playback");
        candidate.stepBeats = (double) o->getProperty ("stepBeats");
        candidate.rateHz = (double) o->getProperty ("rateHz");
        candidate.loopSteps = (int) o->getProperty ("loopSteps");
        if ((candidate.direction != 0 && candidate.direction != 5)
            || candidate.length < 0 || candidate.length > 4 || candidate.playback < 0 || candidate.playback > 2
            || ! std::isfinite (candidate.stepBeats) || candidate.stepBeats <= 0 || candidate.stepBeats > 64
            || ! std::isfinite (candidate.rateHz) || candidate.rateHz < 0 || candidate.rateHz > 100
            || candidate.loopSteps < 0 || candidate.loopSteps > 64) return false;
        out = candidate;
        return true;
    }
};

class UserMotionState
{
public:
    static_assert (std::atomic<float>::is_always_lock_free && std::atomic<double>::is_always_lock_free);
    UserMotionState() { set (UserMotion {}); }
    void set (const UserMotion& next)
    {
        const juce::ScopedLock guard (lock);
        current = next;
        sequence.fetch_add (1);
        for (size_t i = 0; i < points.size(); ++i) points[i].store (next.values[i]);
        steps.store (next.steps);
        direction.store (next.direction);
        smooth.store (next.smooth);
        stepBeats.store (next.stepBeats);
        rateHz.store (next.rateHz);
        loopSteps.store (next.loopSteps);
        sequence.fetch_add (1);
    }
    UserMotion get() const
    {
        const juce::ScopedLock guard (lock);
        return current;
    }
    unsigned revision() const noexcept { return sequence.load(); }
    struct Audio
    {
        std::array<float, 64> values {};
        int steps = 8, direction = 0;
        int loopSteps = 0;
        bool smooth = true;
        double stepBeats = 0.5, rateHz = 0;
        FuncGenPattern pattern() const noexcept
        {
            return { nullptr, steps, direction, smooth, values.data(), nullptr, stepBeats, rateHz };
        }
    };
    void readAudio (Audio& last) const noexcept
    {
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const auto before = sequence.load();
            if ((before & 1u) != 0) continue;
            Audio next;
            for (size_t i = 0; i < points.size(); ++i) next.values[i] = points[i].load();
            next.steps = steps.load();
            next.direction = direction.load();
            next.smooth = smooth.load();
            next.stepBeats = stepBeats.load();
            next.rateHz = rateHz.load();
            next.loopSteps = loopSteps.load();
            if (sequence.load() == before) { last = next; return; }
        }
    }
private:
    mutable juce::CriticalSection lock;
    UserMotion current;
    std::array<std::atomic<float>, 64> points {};
    std::atomic<int> steps { 8 }, direction { 0 };
    std::atomic<bool> smooth { true };
    std::atomic<double> stepBeats { 0.5 }, rateHz { 0 };
    std::atomic<int> loopSteps { 0 };
    std::atomic<unsigned> sequence { 0 };
};

struct MotionLibrary
{
    static juce::File directory()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("TRENCH/Movements");
    }
    static std::vector<UserMotion> load (const juce::File& root = directory())
    {
        std::vector<UserMotion> motions;
        auto files = root.findChildFiles (juce::File::findFiles, false, "*.trenchmove");
        files.sort();
        for (const auto& file : files)
        {
            if (file.getSize() > 32768) continue;
            UserMotion motion;
            if (UserMotion::fromJson (juce::JSON::parse (file.loadFileAsString()), motion)) motions.push_back (motion);
        }
        std::sort (motions.begin(), motions.end(), [] (const auto& a, const auto& b) { return a.name.compareIgnoreCase (b.name) < 0; });
        return motions;
    }
    static juce::Result save (UserMotion& motion, const juce::String& requestedName, const juce::File& root = directory())
    {
        const auto name = requestedName.trim();
        if (name.isEmpty() || name.length() > 32) return juce::Result::fail ("Give it a name (up to 32 characters).");
        auto result = root.createDirectory();
        if (result.failed()) return result;
        const auto existing = load (root);
        auto unique = name;
        for (int suffix = 2; std::any_of (existing.begin(), existing.end(), [&] (const auto& p) { return p.name.equalsIgnoreCase (unique); }); ++suffix)
            unique = name.substring (0, 27) + " " + juce::String (suffix);
        auto saved = motion;
        saved.name = unique;
        saved.dirty = false;
        UserMotion validated;
        if (! UserMotion::fromJson (saved.toJson(), validated)) return juce::Result::fail ("This movement cannot be saved.");
        const auto target = root.getChildFile (juce::Uuid().toString() + ".trenchmove");
        juce::TemporaryFile temporary (target);
        if (! temporary.getFile().replaceWithText (juce::JSON::toString (saved.toJson()))
            || ! temporary.overwriteTargetFileWithTemporary())
            return juce::Result::fail ("Couldn't write the movement file.");
        motion = saved;
        return juce::Result::ok();
    }
};
}

