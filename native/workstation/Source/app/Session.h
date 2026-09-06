#pragma once

#include "Audio.h"
#include "Library.h"
#include "Strip.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace hs
{
class Session
{
public:
    Session (const juce::File& root, const juce::File& stripFile, bool withAudio);
    ~Session();

    juce::File root, stripFile;
    std::vector<Entry> library;
    Strip strip;
    std::vector<Strip> history, future;
    int librarySelected = 0;
    Words words {};
    bool sounding = false;
    juce::String status;
    bool playing = false;
    int source = 1;
    Audio audio;
    bool withAudio = false;
    std::function<void()> onChange;

    void hear (int k);
    void place();
    void keep();
    void walk (double dm, double dq);
    void jump (int k);
    void moveAnchor (int direction);
    void removeAnchor();
    void undo();
    void redo();
    void setPosition (int square, double morph, double q);
    juce::File write (juce::File path = {});
    void setPlaying (bool on);
    void setSource (int s);
    bool key (const juce::KeyPress& k);

private:
    void apply (Strip s, bool edit);
    void audition();
    void changed();
};
}
