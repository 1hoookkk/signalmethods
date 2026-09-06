#pragma once

#include "Audio.h"
#include "Library.h"
#include "Quad.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace hs
{
class Session
{
public:
    Session (const juce::File& root, const juce::File& file, bool withAudio);
    ~Session();

    struct Snapshot { Quad quad; std::vector<Star> added; };
    static constexpr int kCornerPin[4] = { 2, 3, 0, 1 };
    static constexpr const char* kCornerLetters = "ABCD";
    static constexpr int kMade = -3, kPair = -2;

    juce::File root, file;
    std::vector<Star> stars;
    size_t libraryCount = 0;
    Quad quad;
    std::vector<Snapshot> history, future;
    int hovered = -1, selected = -1, auditioning = -1;
    int pairA = -1, pairB = -1;
    double pairT = 0.0;
    int editing = -1;
    Star made;
    bool madeLive = false;
    Words words {};
    bool sounding = false;
    juce::String status;
    bool playing = false;
    int source = 1;
    Audio audio;
    bool withAudio = false;
    std::function<void()> onChange;

    void hover (int k);
    void unhover();
    void select (int k);
    void morphPair (int a, int b, double t);
    bool inPair() const { return auditioning == kPair; }
    bool inMade() const { return auditioning == kMade; }
    void setMade (double f1, double f2);
    void pin (int n, int star);
    void pinCorner (int corner, int star);
    void pinAll (const std::array<int, 4>& pins);
    int addRead (const juce::File& wav);
    void edit (int corner);
    void beginRowEdit();
    void setRow (int corner, int row, Row r);
    Words editWords() const;
    void setPuck (double morph, double q);
    void nudge (double dm, double dq);
    void keep();
    void removeAdded();
    void undo();
    void redo();
    juce::File write (juce::File path = {});
    void setPlaying (bool on);
    void setSource (int s);
    bool key (const juce::KeyPress& k);
    juce::String pinName (int n) const;
    juce::String cornerName (int corner) const { return pinName (kCornerPin[corner]); }
    int currentStar() const;
    int starNamed (const juce::String& name) const;
    juce::String uniqueName (const juce::String& name) const;

private:
    Snapshot snapshot() const;
    void restore (const Snapshot& s);
    void apply();
    void audition();
    void changed();
};
}
