#include <juce_gui_basics/juce_gui_basics.h>
#include "Workstation.h"
#include <cstdio>

namespace
{
juce::File workspaceRoot()
{
    return juce::File (TRENCH_TABLE_STITCH_ROOT);
}

void loadLibrary (ws::Library& lib)
{
    const auto json = workspaceRoot().getChildFile ("native/python/workstation/frames_3d.json");
    if (! (json.existsAsFile() && lib.loadJson (json)))
        lib.loadBodies (workspaceRoot().getChildFile ("plugin/presets/p2k"));
    lib.triangulate();
}

int shoot (const juce::String& path)
{
    ws::Library lib;
    loadLibrary (lib);
    ws::Workstation view (lib);
    for (int i = 0; i < 8; ++i)
        view.cube.corner[(size_t) i] = juce::jmin (i, (int) lib.frames.size() - 1);
    view.cube.morph = 0.3;
    view.cube.q = 0.7;
    view.cube.z = 0.45;
    view.probe = std::pair { 0.55, 0.6 };
    juce::Image img (juce::Image::RGB, view.getWidth(), view.getHeight(), true);
    {
        juce::Graphics g (img);
        view.paintEntireComponent (g, false);
    }
    juce::File out (path);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s  %dx%d  frames %d  triangles %d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight(), (int) lib.frames.size(), (int) lib.tris.size());
    return 0;
}
}

class WorkstationApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "TRENCH Workstation"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        const auto args = juce::StringArray::fromTokens (commandLine, true);
        const int shot = args.indexOf ("--shot");
        if (shot >= 0 && shot + 1 < args.size())
        {
            shoot (args[shot + 1].unquoted());
            quit();
            return;
        }
        loadLibrary (library);
        window = std::make_unique<MainWindow> (getApplicationName(), library);
    }

    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, ws::Library& lib)
            : DocumentWindow (name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new ws::Workstation (lib), true);
            setResizable (true, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    ws::Library library;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (WorkstationApp)
