#include "ui/Look.h"
#include "ui/Screen.h"
#include "ui/Spectrogram.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
class Glass : public juce::DocumentWindow
{
public:
    explicit Glass (hs::Audio& audio)
        : juce::DocumentWindow ("spectrogram", hs::Look::ground, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new hs::Spectrogram (&audio), true);
        setResizable (true, false);
        centreWithSize (900, 560);
    }
    void closeButtonPressed() override { setVisible (false); }
};

class Window : public juce::DocumentWindow
{
public:
    Window (hs::Session& session)
        : juce::DocumentWindow ("HEADSPACE", hs::Look::ground, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new hs::Screen (session), true);
        setResizable (true, false);
        setResizeLimits (1000, 640, 4000, 3000);
        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        const int w = std::min (1120, area.getWidth() - 40), h = std::min (700, area.getHeight() - 80);
        centreWithSize (w, h);
        setVisible (true);
        getContentComponent()->grabKeyboardFocus();
        if (auto* screen = dynamic_cast<hs::Screen*> (getContentComponent()))
            screen->onSpectrogram = [this, &session]
            {
                if (glass == nullptr) glass = std::make_unique<Glass> (session.audio);
                glass->setVisible (! glass->isVisible());
                if (glass->isVisible()) glass->toFront (true);
            };
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

private:
    std::unique_ptr<Glass> glass;
};

class App : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "HEADSPACE"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&look);
        const juce::File root (TRENCH_TABLE_STITCH_ROOT);
        session = std::make_unique<hs::Session> (root, root.getChildFile ("native/workstation/banks/HEADSPACE.quad.json"), true);
        window = std::make_unique<Window> (*session);
    }

    void shutdown() override
    {
        window.reset();
        session.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

private:
    hs::Look look;
    std::unique_ptr<hs::Session> session;
    std::unique_ptr<Window> window;
};
}

START_JUCE_APPLICATION (App)
