#include "Web.h"
#include "dsp/Formants.h"

namespace hs
{
namespace
{
std::vector<std::byte> bytesOf (const juce::File& file)
{
    juce::MemoryBlock mb;
    file.loadFileAsData (mb);
    const auto* p = static_cast<const std::byte*> (mb.getData());
    return { p, p + mb.getSize() };
}
}

Web::Web (Session& s, juce::File dir, juce::File interop)
    : bridge (s), webDir (std::move (dir)), interopJs (std::move (interop))
{
    using Options = juce::WebBrowserComponent::Options;
    const auto userData = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("headspace-webview2");
    auto options = Options{}
        .withBackend (Options::Backend::webview2)
        .withWinWebView2Options (Options::WinWebView2{}.withUserDataFolder (userData).withBackgroundColour (juce::Colours::transparentBlack))
        .withNativeIntegrationEnabled()
        .withNativeFunction ("state", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion done) { done (bridge.state()); })
        .withNativeFunction ("dispatch", [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion done)
        {
            juce::Array<juce::var> rest;
            for (int i = 1; i < args.size(); ++i) rest.add (args[i]);
            const bool ok = args.size() > 0 && bridge.dispatch (args[0].toString(), rest);
            done (juce::var (ok));
        })
        .withNativeFunction ("spectrogram", [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion done) { if (onSpectrogram) onSpectrogram(); done (juce::var (true)); })
        .withResourceProvider ([this] (const juce::String& path) { return resource (path); });
    view = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*view);
    view->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    stamp = newestStamp();
    s.onChange = [this] { dirty = true; };
    startTimerHz (30);
}

juce::int64 Web::newestStamp() const
{
    juce::int64 newest = 0;
    for (const auto& f : webDir.findChildFiles (juce::File::findFiles, false)) newest = std::max (newest, f.getLastModificationTime().toMilliseconds());
    return newest;
}

Web::~Web() { stopTimer(); }

void Web::resized() { view->setBounds (getLocalBounds()); }

void Web::timerCallback()
{
    auto& session = bridge.session;
    if (session.withAudio && session.audio.isOpen())
    {
        const int n = session.audio.pull (tap.data(), (int) tap.size());
        for (int i = 0; i < n; ++i) lpc.gal (tap[(size_t) i]);
        if (n > 0 && ticks % 3 == 0)
        {
            const auto f = lpcFormants (lpc.lpc.k, session.audio.sampleRate());
            if (f[0] != bridge.markF1 || f[1] != bridge.markF2) { bridge.markF1 = f[0]; bridge.markF2 = f[1]; dirty = true; }
        }
    }
    if (++ticks % 15 == 0)
    {
        const auto now = newestStamp();
        if (now != stamp) { stamp = now; view->goToURL (juce::WebBrowserComponent::getResourceProviderRoot()); return; }
    }
    if (! dirty && ticks % 30 != 0) return;
    dirty = false;
    view->emitEventIfBrowserIsVisible ("state", bridge.state());
}

std::optional<juce::WebBrowserComponent::Resource> Web::resource (const juce::String& path) const
{
    juce::String p = path;
    if (p.isEmpty() || p == "/") p = "/index.html";
    if (p == "/juce/index.js") return juce::WebBrowserComponent::Resource { bytesOf (interopJs), "text/javascript" };
    const auto file = webDir.getChildFile (p.substring (1));
    if (! file.existsAsFile()) return std::nullopt;
    const auto ext = file.getFileExtension();
    const juce::String mime = ext == ".html" ? "text/html" : ext == ".js" ? "text/javascript" : ext == ".css" ? "text/css" : "application/octet-stream";
    return juce::WebBrowserComponent::Resource { bytesOf (file), mime };
}
}
