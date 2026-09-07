#include "ui/Look.h"
#include "ui/Spectrogram.h"
#include "ui/Web.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
class Glass : public juce::DocumentWindow
{
public:
    explicit Glass (hs::Session& session)
        : juce::DocumentWindow ("reading room", hs::Look::ground, juce::DocumentWindow::allButtons), owner (session)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new hs::Spectrogram (&session.audio, &session), true);
        setResizable (true, false);
        centreWithSize (1100, 620);
    }
    void closeButtonPressed() override { setVisible (false); }
    void visibilityChanged() override { owner.setReadingRoom (isVisible()); }

private:
    hs::Session& owner;
};

class Window : public juce::DocumentWindow
{
public:
    Window (hs::Session& session)
        : juce::DocumentWindow ("HEADSPACE", hs::Look::ground, juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (false);
        setTitleBarHeight (18);
        const juce::File root (TRENCH_TABLE_STITCH_ROOT);
        setContentOwned (new hs::Web (session, root.getChildFile ("native/workstation/Source/web"), juce::File (TRENCH_JUCE_INTEROP_JS)), true);
        setResizable (true, false);
        setResizeLimits (180, 194, 600, 614);
        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        setBounds (area.getRight() - 200, area.getY() + 40, 180, 194);
        setAlwaysOnTop (true);
        setVisible (true);
        getContentComponent()->grabKeyboardFocus();
        if (auto* screen = dynamic_cast<hs::Web*> (getContentComponent()))
            screen->onSpectrogram = [this, &session]
            {
                if (glass == nullptr) glass = std::make_unique<Glass> (session);
                glass->setVisible (! glass->isVisible());
                if (glass->isVisible())
                {
                    glass->toFront (true);
                    if (auto* content = glass->getContentComponent()) content->grabKeyboardFocus();
                }
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
