#include "Screen.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
class Window : public juce::DocumentWindow
{
public:
    Window (hs::Session& session)
        : juce::DocumentWindow ("HEADSPACE", juce::Colour (0xff0a0e0b), juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new hs::Screen (session), true);
        setResizable (true, false);
        setResizeLimits (820, 520, 4000, 3000);
        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        const int w = std::min (1120, area.getWidth() - 40), h = std::min (700, area.getHeight() - 80);
        centreWithSize (w, h);
        setVisible (true);
        getContentComponent()->grabKeyboardFocus();
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class App : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "HEADSPACE"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        const juce::File root (TRENCH_TABLE_STITCH_ROOT);
        session = std::make_unique<hs::Session> (root, root.getChildFile ("native/workstation/banks/HEADSPACE.quad.json"), true);
        window = std::make_unique<Window> (*session);
    }

    void shutdown() override
    {
        window.reset();
        session.reset();
    }

private:
    std::unique_ptr<hs::Session> session;
    std::unique_ptr<Window> window;
};
}

START_JUCE_APPLICATION (App)
