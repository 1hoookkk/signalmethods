#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Workstation.h"
#include <cstdio>

namespace
{
juce::File workspaceRoot() { return juce::File (TRENCH_TABLE_STITCH_ROOT); }

void loadLibrary (ws::Library& lib)
{
    const auto json = workspaceRoot().getChildFile ("native/python/workstation/frames_3d.json");
    if (! (json.existsAsFile() && lib.loadJson (json)))
        lib.loadBodies (workspaceRoot().getChildFile ("plugin/presets/p2k"));
    lib.computePca();
    lib.sort();
}

int shoot (const juce::String& path)
{
    ws::Library lib;
    loadLibrary (lib);
    ws::Workstation view (lib, false);
    view.exportDir = workspaceRoot().getChildFile ("plugin/presets/user");
    view.workspace = workspaceRoot();
    view.demo();
    view.exportBody (juce::File (path).getSiblingFile ("ws_demo.body240"));
    juce::Image img (juce::Image::RGB, view.getWidth(), view.getHeight(), true);
    {
        juce::Graphics g (img);
        view.paintEntireComponent (g, false);
    }
    juce::File out (path);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    view.closeEditor();
    view.openSpectro (0, 0.6);
    view.demoRegion (0.9, 1.7);
    juce::Image img3 (juce::Image::RGB, view.getWidth(), view.getHeight(), true);
    {
        juce::Graphics g (img3);
        view.paintEntireComponent (g, false);
    }
    auto out3 = out.getSiblingFile (out.getFileNameWithoutExtension() + "_sound.png");
    out3.deleteFile();
    juce::FileOutputStream os3 (out3);
    juce::PNGImageFormat().writeImageToStream (img3, os3);
    view.spectro = false;
    juce::Image img2 (juce::Image::RGB, view.getWidth(), view.getHeight(), true);
    {
        juce::Graphics g (img2);
        view.paintEntireComponent (g, false);
    }
    auto out2 = out.getSiblingFile (out.getFileNameWithoutExtension() + "_field.png");
    out2.deleteFile();
    juce::FileOutputStream os2 (out2);
    juce::PNGImageFormat().writeImageToStream (img2, os2);
    std::printf ("wrote %s  %dx%d  frames %d  anchors %d  triangles %d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight(), (int) lib.frames.size(), (int) lib.anchors.size(), (int) lib.tris.size());
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
        if (shot >= 0 && shot + 1 < args.size()) { shoot (args[shot + 1].unquoted()); quit(); return; }
        loadLibrary (library);
        window = std::make_unique<MainWindow> (getApplicationName(), library);
        if (auto* w = dynamic_cast<ws::Workstation*> (window->getContentComponent())) { w->exportDir = workspaceRoot().getChildFile ("plugin/presets/user"); w->workspace = workspaceRoot(); }
    }

    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, ws::Library& lib) : DocumentWindow (name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new ws::Workstation (lib, true, true), true);
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
