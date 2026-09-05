#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include "../model/Library.h"
#include "../model/Stitch.h"
#include "../model/Timeline.h"
#include "../model/Body.h"
#include "../model/Sound.h"
#include "../model/Morph.h"
#include "../render/GLRenderer.h"
#include "../render/Canvas.h"
#include "Audio.h"
#include "Layout.h"
#include "View3D.h"
#include <optional>

namespace ws
{
class Workstation : public juce::Component, public juce::OpenGLRenderer, private juce::Timer
{
public:
    Workstation (Library& library, Stitch& stitch, bool useGL, bool withAudio = false);
    ~Workstation() override;

    void demo();
    void demoSound (int wavIndex, double at, double regionA, double regionB);
    void demoPair (double t);
    void demoShelf();
    void demoMorph();
    void setRoom (Room r);
    bool exportBody (const juce::File& file);
    juce::File exportDir, workspace;
    struct Probe { bool free = false, sounding = false, listening = false, comparing = false; double x = 0.0, y = 0.0, z = 0.0, morph = 0.0, q = 0.0; int playFrame = -1, room = 0; std::array<int, 4> corners { -1, -1, -1, -1 }; juce::String status; };
    juce::Point<float> scanPoint (double fraction) const;
    Probe probe() const;
    juce::Rectangle<float> keyBox (const juce::String& id) const;
    juce::Rectangle<float> fieldRect() const { return L.field; }
    juce::Point<float> clearPoint() const;
    juce::Rectangle<float> padBox() const { return padRect(); }
    void gesture (juce::Point<float> p, int phase, bool shift = false, bool right = false);

    void paint (juce::Graphics& g) override;
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    enum class Mode { none, scan, pad, holdCompare, surface, lift, orbit, scrub, dragFrame, dragSpot, dragKey, pickHz, pair, wheel, dragPole, dragZero, open, slice, region };
    struct KeyBox { juce::String id, label; juce::Rectangle<float> box; bool on; };
    struct TrayRow { int frame; int group; bool header; };

    Library& lib;
    Stitch& st;
    Timeline tl;
    Body body;
    Sound sound;
    Layout L;
    View3D view;
    bool gl;
    juce::OpenGLContext ctx;
    GLRenderer renderer;
    std::unique_ptr<Audio> audio;
    std::optional<Spot> spot, picked, dragSpot;
    int openFace = -1;
    std::array<bool, 3> sweep { false, false, false };
    std::vector<int> stripOrder;
    bool stripDirty = true, sortNear = false, fine = false, compare = false;
    std::array<bool, 4> chosen { false, false, false, false };
    int stripScroll = 0, selectedSlot = -1, playFrame = -1;
    double padPressM = 0.0, padPressQ = 0.0, padPressM0 = 0.0, padPressQ0 = 0.0;
    double scanPos = 0.0;
    int regionDragEnd = 1;
    bool sourceSample = false, analyse = false;
    std::vector<float> measured;
    void updateMeasured();
    void buildStrip();
    juce::Rectangle<float> scanRect() const;
    double scanAt (juce::Point<float> p) const;
    Words scanWords() const;
    int scanFrame() const;
    void setScan (juce::Point<float> p);
    void takeScan();
    void fillCorners();
    void paintStrip (Canvas& g);
    juce::Rectangle<float> padRect() const;
    void setPad (juce::Point<float> p);
    void paintPad (Canvas& g);
    double sweepT = 0.0;
    double freeZ = 0.5;
    int anchorAt (juce::Point<float> p) const;
    void grab (int item);
    std::array<bool, kGroups> open { true, false, true, true, true, true, true };
    juce::String status;
    Mode mode = Mode::none;
    int dragFrame = -1, dragKey = -1, trayScroll = 0, wavScroll = 0;
    bool pairMode = false;
    int pairA = -1, pairB = -1;
    double pairT = 0.0;
    bool playBody = false, wet = true;
    int editCorner = 0, editing = -1, dragStage = -1, copyFrom = -1;
    double openAmount = 0.5, fieldHz = 1000.0;
    std::array<bool, kRows> lockRow { false, false, false, false, false, false };
    juce::Rectangle<float> openBar;
    juce::Point<float> dragPos, dragStart;
    double orbitAz = 0.0, orbitEl = 0.0;
    double playT0 = 0.0, playFrom = 0.0;
    std::vector<KeyBox> keys;
    std::vector<TrayRow> trayRows;
    std::vector<juce::File> wavs;

    void timerCallback() override;
    void redraw();
    void feedAudio();
    void layoutKeys();
    void buildTray();
    bool floorOpen (int floor) const;
    bool haveSound() const;
    Words playingWords() const;
    Morph live() const;
    bool pairLive() const;
    Vec3 pairPoint (double t) const;
    void pairTo (double t);
    void pairFromPoint (juce::Point<float> p);
    juce::String pairName() const;
    juce::String nodeName (int node) const;
    void groupMean (int group);
    int frameFor (const Spot& s);
    int captureSpot (const Spot& s, const Words& words, const juce::String& name);
    Spot spotAt (juce::Point<float> p) const;
    int nodeAt (juce::Point<float> p) const;
    int faceAt (juce::Point<float> p) const;
    int faceAtUnused (juce::Point<float> p) const;
    void openFilter (int face);
    void setSurface (juce::Point<float> p);
    void setPairT (juce::Point<float> p);
    void setWheel (juce::Point<float> p);
    void scrubTo (float x);
    void pickHz (float x);
    void press (const juce::String& id);
    void capture();
    void assignCorner (int i);
    void openEditor (int corner);
    void dragHandle (juce::Point<float> p);
    int f1Row (const Frame& f) const;
    void setOpen (float x);
    void sharpenQ();
    void ceiling();
    void loadWav (int index);
    void setSlice (float x);
    void setRegion (float x, bool start);
    void frameFromSlice();
    int trayAt (juce::Point<float> p) const;
    int keyAt (juce::Point<float> p) const;
    std::vector<Batch> scene() const;
    void sceneStitch (std::vector<Batch>& out) const;
    std::vector<Batch> frame();
    void paintPanels (Canvas& g);
    void paintChrome (Canvas& g);
    void paintTray (Canvas& g);
    void paintAxes (Canvas& g);
    void paintResponse (Canvas& g);
    void paintBody (Canvas& g);
    void paintArma (Canvas& g);
    void paintEditor (Canvas& g);
    void paintSound (Canvas& g);
    void paintTimeline (Canvas& g);
};
}
