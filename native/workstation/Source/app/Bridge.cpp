#include "Bridge.h"
#include <cmath>

namespace hs
{
Bridge::Bridge (Session& s) : session (s)
{
    for (int i = 0; i < 96; ++i) hz.push_back (20.0 * std::pow (1000.0, i / 95.0));
}

juce::Array<juce::var> Bridge::curveOf (const Words& words) const
{
    juce::Array<juce::var> out;
    for (double db : responseDb (words, hz)) out.add (std::round (std::clamp (db, -60.0, 30.0) * 10.0) / 10.0);
    return out;
}

juce::var Bridge::state() const
{
    auto* d = new juce::DynamicObject();
    d->setProperty ("label", session.playingLabel);
    d->setProperty ("status", session.status);
    d->setProperty ("heard", curveOf (session.words));
    {
        auto* pad = new juce::DynamicObject();
        pad->setProperty ("morph", session.quad.morph);
        pad->setProperty ("q", session.quad.q);
        pad->setProperty ("onCorner", session.onCorner());
        pad->setProperty ("working", session.working);
        pad->setProperty ("editing", session.editing);
        pad->setProperty ("complete", session.quad.complete());
        d->setProperty ("pad", juce::var (pad));
    }
    {
        juce::Array<juce::var> corners;
        for (int c = 0; c < 4; ++c)
        {
            auto* o = new juce::DynamicObject();
            const int pin = session.quad.pins[(size_t) Session::kCornerPin[c]];
            o->setProperty ("letter", juce::String::charToString (Session::kCornerLetters[c]));
            o->setProperty ("name", session.cornerName (c));
            o->setProperty ("star", pin);
            if (pin >= 0 && pin < (int) session.stars.size()) o->setProperty ("curve", curveOf (session.stars[(size_t) pin].words));
            corners.add (juce::var (o));
        }
        d->setProperty ("corners", corners);
    }
    {
        auto* pair = new juce::DynamicObject();
        const bool a = session.pairA >= 0 && session.pairA < (int) session.stars.size();
        const bool b = session.pairB >= 0 && session.pairB < (int) session.stars.size();
        pair->setProperty ("a", a ? session.stars[(size_t) session.pairA].name : juce::String());
        pair->setProperty ("b", b ? session.stars[(size_t) session.pairB].name : juce::String());
        pair->setProperty ("aStar", session.pairA);
        pair->setProperty ("bStar", session.pairB);
        if (a) pair->setProperty ("aCurve", curveOf (session.stars[(size_t) session.pairA].words));
        if (b) pair->setProperty ("bCurve", curveOf (session.stars[(size_t) session.pairB].words));
        pair->setProperty ("morph", session.pairT);
        pair->setProperty ("frequency", session.frequency);
        pair->setProperty ("stress", session.stress);
        pair->setProperty ("octaves", session.octaves);
        pair->setProperty ("live", session.inPair());
        pair->setProperty ("anchorTarget", session.anchorTarget);
        d->setProperty ("pair", juce::var (pair));
    }
    {
        auto* source = new juce::DynamicObject();
        source->setProperty ("which", session.source);
        source->setProperty ("note", session.note);
        source->setProperty ("fixed", session.fixedPitch);
        source->setProperty ("held", session.heldNote);
        source->setProperty ("playing", session.playing);
        source->setProperty ("loop", session.loopName);
        d->setProperty ("source", juce::var (source));
    }
    {
        juce::Array<juce::var> cards;
        for (int k = 0; k < (int) session.stars.size(); ++k)
        {
            const auto& s = session.stars[(size_t) k];
            if (s.kind != "capture" && s.kind != "read" && s.kind != "factory") continue;
            auto* o = new juce::DynamicObject();
            o->setProperty ("star", k);
            o->setProperty ("name", s.name);
            o->setProperty ("kind", s.kind);
            o->setProperty ("family", s.kind == "capture" ? juce::String ("captures") : s.body);
            cards.add (juce::var (o));
        }
        d->setProperty ("cards", cards);
        d->setProperty ("selected", session.selected);
        d->setProperty ("auditioning", session.auditioning);
    }
    return juce::var (d);
}

bool Bridge::dispatch (const juce::String& name, const juce::Array<juce::var>& args)
{
    auto num = [&] (int i, double fallback = 0.0) { return i < args.size() ? (double) args[i] : fallback; };
    auto whole = [&] (int i, int fallback = -1) { return i < args.size() ? (int) args[i] : fallback; };
    if (name == "setPuck") { session.setPuck (num (0), num (1)); return true; }
    if (name == "nudge") { session.nudge (num (0), num (1)); return true; }
    if (name == "toCorner") { session.toCorner (whole (0, 0)); return true; }
    if (name == "pinCorner") { session.pinCorner (whole (0, 0), whole (1)); return true; }
    if (name == "setPair") { session.setPair (whole (0, 0), whole (1)); return true; }
    if (name == "select") { session.select (whole (0)); return true; }
    if (name == "sweep") { session.sweep (num (0)); return true; }
    if (name == "setProbe") { session.setProbe (num (0, session.pairT), num (1, session.frequency), num (2, session.stress)); return true; }
    if (name == "setOctaves") { session.setOctaves (num (0, session.octaves)); return true; }
    if (name == "keep") { session.keep(); return true; }
    if (name == "bake") { session.bake(); return true; }
    if (name == "write") { session.write(); return true; }
    if (name == "noteOn") { session.noteOn (whole (0, 60)); return true; }
    if (name == "noteOff") { session.noteOff(); return true; }
    if (name == "setSource") { session.setSource (whole (0, 1)); return true; }
    if (name == "setTracking") { session.setTracking (args.size() > 0 && (bool) args[0]); return true; }
    if (name == "setPlaying") { session.setPlaying (args.size() > 0 && (bool) args[0]); return true; }
    if (name == "undo") { session.undo(); return true; }
    if (name == "redo") { session.redo(); return true; }
    if (name == "removeAdded") { session.removeAdded(); return true; }
    return false;
}
}
