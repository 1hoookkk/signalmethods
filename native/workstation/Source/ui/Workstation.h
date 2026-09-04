#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include "../model/Library.h"
#include "../model/Timeline.h"
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

    void paint (juce::Graphics& g) override;
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    enum class Mode { none, probe, scrub, dragFrame, dragAnchor, dragKey };
    struct KeyBox { juce::String id, label; juce::Rectangle<float> box; bool on; };

    Library& lib;
    Timeline tl;
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
    void press (const juce::String& id);
    void capture();
    int anchorAt (juce::Point<float> p) const;
    int trayAt (juce::Point<float> p) const;
    int keyAt (juce::Point<float> p) const;
    std::vector<Batch> scene() const;
    void paintChrome (juce::Graphics& g);
};
}
