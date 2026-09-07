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

    struct Snapshot { Quad quad; std::vector<Star> added; double frequency = 0.0, stress = 1.0, octaves = 1.0; Patch patch; };
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
    double frequency = 0.0, stress = 1.0, octaves = 1.0;
    bool fixedPitch = true;
    int soundingPitch = -1;
    int editing = -1;
    int working = 0;
    int anchorTarget = -1;
    int heldNote = -1;
    int samplerNote = -1;
    bool readingRoom = false;
    juce::String familyName;
    int target() const { return editing >= 0 ? editing : working; }
    Star made;
    bool madeLive = false;
    Words words {};
    bool sounding = false;
    juce::String status;
    bool playing = false;
    int source = 1;
    int note = 45;
    Words heard {};
    juce::String playingLabel;
    juce::String loopName;
    Audio audio;
    bool withAudio = false;
    std::function<void()> onChange;

    void hover (int k);
    void unhover();
    void select (int k);
    void morphPair (int a, int b, double t);
    void setPair (int which, int star);
    void sweep (double t);
    void setProbe (double morph, double frequency, double stress);
    void setOctaves (double value);
    bool bake();
    Explore explore() const;
    bool inPair() const { return auditioning == kPair; }
    bool inMade() const { return auditioning == kMade; }
    void setMade (double f1, double f2);
    void setTransposed (int star, double f1);
    void pin (int n, int star);
    void pinCorner (int corner, int star);
    void toCorner (int corner);
    void toColumn (int column);
    bool placeable() const;
    void pinAll (const std::array<int, 4>& pins);
    int addRead (const juce::File& wav);
    int addFrame (const Star& star);
    std::vector<juce::String> families() const;
    bool loadFamily (const juce::String& family);
    void setReadingRoom (bool on);
    int addFit (const juce::File& wav);
    bool refit();
    void edit (int corner);
    void editAnchor (int which);
    int editStar() const;
    bool onCorner() const;
    bool editable() const;
    bool live() const { return anchorTarget < 0 && ! (auditioning == -1 && onCorner()); }
    void setTracking (bool fixed);
    void relevel();
    void placeInTarget (int star);
    void beginRowEdit();
    void setRow (int corner, int row, Row r);
    void setSection (int corner, int row, const Section& section, bool keepFifth = true);
    void setSectionWords (int corner, int row, const trench::core::PackedSection& words, bool balance = true);
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
    void setNote (int midi);
    void noteOn (int midi);
    void noteOff();
    void keyNoteOn (int midi);
    void keyNoteOff (int midi);
    int keyOctave = 48;
    bool setLoop (const juce::File& wav);
    void noteIn (const juce::MidiMessage& m);
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
