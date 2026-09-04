#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include "../model/Library.h"
#include "../model/Timeline.h"
#include "../model/Body.h"
#include "../render/GLRenderer.h"
#include "Layout.h"
#include <optional>

namespace ws
{
class Workstation : public juce::Component, public juce::OpenGLRenderer, private juce::Timer
{
public:
    Workstation (Library& library, bool useGL);
    ~Workstation() override;

    void demo();
    void closeEditor() { editing = -1; }
    bool exportBody (const juce::File& file);
    juce::File exportDir;

    void paint (juce::Graphics& g) override;
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    enum class Mode { none, probe, scrub, dragFrame, dragAnchor, dragKey, pickHz, pair, wheel, dragPole, dragZero, open };
    struct KeyBox { juce::String id, label; juce::Rectangle<float> box; bool on; };

    Library& lib;
    Timeline tl;
    Body body;
    bool playBody = false;
    int editCorner = 0;
    int editing = -1;
    int dragStage = -1;
    double openAmount = 0.5;
    juce::Rectangle<float> openBar;
    void setOpen (float x);
    int f1Row (const Frame& f) const;
    void openEditor (int corner);
    void dragHandle (juce::Point<float> p);
    void setWheel (juce::Point<float> p);
    void assignCorner (int i);
    Words playingWords() const;
    Layout L;
    bool gl;
    juce::OpenGLContext ctx;
    GLRenderer renderer;
    std::optional<std::array<double, 2>> probe;
    int lit = -1;
    juce::String status;
    Mode mode = Mode::none;
    int dragFrame = -1, dragAnchor = -1, dragKey = -1, trayScroll = 0, pickFor = -1;
    juce::Point<float> dragPos, dragStart;
    double playT0 = 0.0, playFrom = 0.0;
    std::vector<KeyBox> keys;

    void timerCallback() override;
    void redraw();
    void layoutKeys();
    std::optional<Blend> current() const;
    void setProbe (juce::Point<float> p);
    void scrubTo (float x);
    void pickHz (float x);
    void press (const juce::String& id);
    void capture();
    int anchorAt (juce::Point<float> p) const;
    int trayAt (juce::Point<float> p) const;
    int keyAt (juce::Point<float> p) const;
    std::vector<Batch> scene() const;
    FieldMesh fieldMesh() const;
    double fieldHz = 1000.0;
    bool showSurface = false;
    bool pairMode = false;
    int pairA = -1, pairB = -1;
    double pairT = 0.0;
    void setPairT (juce::Point<float> p);
    void paintChrome (juce::Graphics& g);
};
}
