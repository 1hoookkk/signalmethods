#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Workstation.h"
#include <algorithm>
#include <cstdio>

namespace
{
juce::File workspaceRoot() { return juce::File (TRENCH_TABLE_STITCH_ROOT); }

void loadStitch (ws::Stitch& st, ws::Library& lib)
{
    st.loadJson (workspaceRoot().getChildFile ("native/python/workstation/stitch.json"));
    std::vector<juce::String> have;
    for (const auto& s : st.stubs) have.push_back (s.name);
    for (int i = 0; i < (int) lib.frames.size(); ++i)
    {
        const auto& f = lib.frames[(size_t) i];
        if (f.group == 0 || f.group == 1 || std::find (have.begin(), have.end(), f.name) != have.end()) continue;
        const int stub = st.addStub (f.name, f.words, st.nearestNode (f.words), f.group);
        st.stubs[(size_t) stub].frame = i;
    }
    st.placeOnGrid();
}

void loadLibrary (ws::Library& lib)
{
    const auto json = workspaceRoot().getChildFile ("native/python/workstation/frames_3d.json");
    if (! (json.existsAsFile() && lib.loadJson (json)))
        lib.loadBodies (workspaceRoot().getChildFile ("plugin/presets/p2k"));
    lib.addSchwa();
    for (const auto& f : workspaceRoot().getChildFile ("native/python/workstation/chords").findChildFiles (juce::File::findFiles, false, "*.json")) lib.loadChords (f);
    lib.computePca();
    lib.sort();
}

int shoot (const juce::String& path)
{
    ws::Library lib;
    ws::Stitch st;
    loadLibrary (lib);
    loadStitch (st, lib);
    ws::Workstation view (lib, st, false);
    view.exportDir = workspaceRoot().getChildFile ("plugin/presets/user");
    view.workspace = workspaceRoot();
    view.demo();
    view.exportBody (juce::File (path).getSiblingFile ("ws_demo.body240"));
    juce::Image img (juce::Image::RGB, view.getWidth() * 2, view.getHeight() * 2, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        view.paintEntireComponent (g, false);
    }
    juce::File out (path);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    view.demoSound (0, 0.6, 0.9, 1.7);
    juce::Image img3 (juce::Image::RGB, view.getWidth() * 2, view.getHeight() * 2, true);
    {
        juce::Graphics g (img3);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        view.paintEntireComponent (g, false);
    }
    auto out3 = out.getSiblingFile (out.getFileNameWithoutExtension() + "_sound.png");
    out3.deleteFile();
    juce::FileOutputStream os3 (out3);
    juce::PNGImageFormat().writeImageToStream (img3, os3);
    view.setRoom (ws::Room::frames);
    juce::Image img2 (juce::Image::RGB, view.getWidth() * 2, view.getHeight() * 2, true);
    {
        juce::Graphics g (img2);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        view.paintEntireComponent (g, false);
    }
    auto out2 = out.getSiblingFile (out.getFileNameWithoutExtension() + "_field.png");
    out2.deleteFile();
    juce::FileOutputStream os2 (out2);
    juce::PNGImageFormat().writeImageToStream (img2, os2);
    view.demoPair (2.6);
    juce::Image img4 (juce::Image::RGB, view.getWidth() * 2, view.getHeight() * 2, true);
    {
        juce::Graphics g (img4);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        view.paintEntireComponent (g, false);
    }
    auto out4 = out.getSiblingFile (out.getFileNameWithoutExtension() + "_pair.png");
    out4.deleteFile();
    juce::FileOutputStream os4 (out4);
    juce::PNGImageFormat().writeImageToStream (img4, os4);
    view.demoShelf();
    juce::Image img5 (juce::Image::RGB, view.getWidth() * 2, view.getHeight() * 2, true);
    {
        juce::Graphics g (img5);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        view.paintEntireComponent (g, false);
    }
    auto out5 = out.getSiblingFile (out.getFileNameWithoutExtension() + "_shelf.png");
    out5.deleteFile();
    juce::FileOutputStream os5 (out5);
    juce::PNGImageFormat().writeImageToStream (img5, os5);
    std::printf ("wrote %s  %dx%d  frames %d  nodes %d  edges %d  faces %d  stubs %d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight(), (int) lib.frames.size(), (int) st.nodes.size(), (int) st.edges.size(), (int) st.faces.size(), (int) st.stubs.size());
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
        loadStitch (stitch, library);
        window = std::make_unique<MainWindow> (getApplicationName(), library, stitch);
        if (auto* w = dynamic_cast<ws::Workstation*> (window->getContentComponent())) { w->exportDir = workspaceRoot().getChildFile ("plugin/presets/user"); w->workspace = workspaceRoot(); }
    }

    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, ws::Library& lib, ws::Stitch& st) : DocumentWindow (name, juce::Colours::black, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new ws::Workstation (lib, st, true, true), true);
            setResizable (true, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    ws::Library library;
    ws::Stitch stitch;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (WorkstationApp)
