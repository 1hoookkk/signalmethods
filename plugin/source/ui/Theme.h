#pragma once
#include "../UiLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdlib>
namespace trench::ui
{
inline constexpr int   kEditorWidth        = 326;
inline constexpr int   kEditorHeight       = 503;
inline constexpr int   kEditorHeightClosed = 361;

inline constexpr int   kBayValueWidth      = 46;
inline constexpr int   kBaySourceWidth     = 116;
inline constexpr int   kBaySourceHeight    = 20;

inline constexpr float kLabelPt            = 12.0f;
inline constexpr float kValuePt            = 13.0f;
inline constexpr int   kRowH               = 26;
inline constexpr float kBayValuePt         = kValuePt;
inline constexpr int   kBayValueHeight     = 18;

inline constexpr float kBayKnobDiameter    = 32.0f;
inline constexpr float kPanelSourceWidth   = 1010.0f;
inline constexpr float kPanelSourceHeight  = 1557.0f;
inline juce::Rectangle<float> sourceRectToEditor (juce::Rectangle<float> s)
{
    return { s.getX() * kEditorWidth  / kPanelSourceWidth,
             s.getY() * kEditorHeight / kPanelSourceHeight,
             s.getWidth()  * kEditorWidth  / kPanelSourceWidth,
             s.getHeight() * kEditorHeight / kPanelSourceHeight };
}
inline const char* const kUiFontName = "Tahoma";
inline const char* const kUiEmphasisFontName = "Tahoma";
inline juce::String& uiFontFamily()
{
    static juce::String* family = new juce::String (kUiFontName);
    return *family;
}
inline juce::String& uiEmphasisFontFamily()
{
    static juce::String* family = new juce::String (kUiEmphasisFontName);
    return *family;
}
inline bool& uiBoldEnabled()
{
    static bool enabled = false;
    return enabled;
}
inline juce::Font displayFont (float height, bool emphasis = false)
{
    const bool useSyntheticBold = emphasis && uiBoldEnabled();
    const auto& family = emphasis ? uiEmphasisFontFamily() : uiFontFamily();
    return juce::Font (juce::FontOptions (family, height,
                                          useSyntheticBold ? juce::Font::bold : juce::Font::plain));
}
inline constexpr float kPlateWordPt    = kLabelPt;
inline constexpr float kMicroCaptionPt = 8.6f;
inline juce::Font telemetryFont (float height, bool emphasis = false)
{

    return displayFont (height, emphasis);
}
struct Theme
{
    const trench::UiLayout& layout;
    juce::Colour accent()      const { return layout.colour ("accent",      juce::Colour (0xff3cc8be)); }
    juce::Colour curveColour() const { return layout.colour ("curveColour", juce::Colour (0xff3cc8be)); }
    juce::Colour curveHighlight() const { return layout.colour ("curveHighlight", juce::Colour (0xffe6fff4)); }
    juce::Colour telemetry() const { return layout.colour ("telemetry", juce::Colour (0xff608074)); }
    juce::Colour rollerIllumination() const { return layout.colour ("rollerIllumination", juce::Colour (0xff3cc8be)); }
    juce::Colour modulationLamp() const { return layout.colour ("modulationLamp", juce::Colour (0xff3cc8be)); }
    juce::Colour amber()       const { return layout.colour ("amber",       juce::Colour (0xffa9554e)); }
    juce::Colour wellTop()     const { return layout.colour ("wellTop",     juce::Colour (0xffe7dec9)); }
    juce::Colour wellBottom()  const { return layout.colour ("wellBottom",  juce::Colour (0xffc9c0a8)); }
    juce::Colour wellKeyline() const { return layout.colour ("wellKeyline", juce::Colour (0xff5c4f3a)); }
    juce::Colour bevelHi()     const { return layout.colour ("bevelHi",     juce::Colour (0x88e7dec9)); }
    juce::Colour bevelLo()     const { return layout.colour ("bevelLo",     juce::Colour (0x3d000000)); }
    juce::Colour arrow()       const { return layout.colour ("arrow",       juce::Colour (0xff241e15)); }
    juce::Colour rim()         const { return layout.colour ("rim",         juce::Colour (0xff241e15)); }
    juce::Colour screenEdge()  const { return layout.colour ("screenEdge",  juce::Colour (0xff171325)); }
    juce::Colour labelInk()    const { return layout.colour ("labelInk",    juce::Colour (0xff24231f)); }
    juce::Colour phosphor()    const { return layout.colour ("phosphor",    juce::Colour (0xff1a1624)); }
    juce::Colour spectrumGhost() const { return layout.colour ("spectrumGhost", juce::Colour (0xff9fcfc4)); }
    float  wellRadius()        const { return (float) layout.param ("wellRadius", 9.0); }
    float  componentRadius()   const { return (float) layout.param ("componentRadius", 2.5); }
    float  readoutAliasScale() const { return (float) juce::jlimit (0.3, 1.0, layout.param ("readoutAliasScale", 0.72)); }
    float  typeArrowExtra()    const { return (float) layout.param ("typeArrowExtra", 6.0); }
    double curveDbTop()        const { return layout.param ("curveDbTop", 18.0); }
    double curveDbBottom()     const { return layout.param ("curveDbBottom", -30.0); }
    juce::Rectangle<float> rect (const juce::String& id) const
    {
        return sourceRectToEditor (layout.sourceRectFor (id));
    }

    juce::Rectangle<float> sourceRect (const juce::String& id) const
    {
        return layout.sourceRectFor (id);
    }
    float    fontSize (const juce::String& id, float fb) const
    {
        constexpr float fontReferenceWidth = 440.0f;
        return layout.fontSizeFor (id).value_or (fb) * (float) kEditorWidth / fontReferenceWidth;
    }
    juce::Font smallLabel (bool micro = false) const
    {
        return displayFont (micro ? kMicroCaptionPt : kPlateWordPt, true);
    }
    juce::Colour textColour (const juce::String& id, juce::Colour fb) const { return layout.textColourFor (id).value_or (fb); }
    juce::String text (const juce::String& id, const juce::String& fb) const { return layout.textFor (id, fb); }
    float    opacity (const juce::String& id) const { return layout.opacityFor (id); }
};

inline void drawFrostedGlassControl (juce::Graphics& g, juce::Rectangle<float> r,
                                     float radius, bool isActive, const Theme& t)
{
        const auto face = r.reduced (0.35f);
    const auto top    = juce::Colour (0xffc3d1df);
    const auto middle = juce::Colour (0xffb2c3d5);
    const auto bottom = juce::Colour (0xffa5b8cc);
    const float faceRad = juce::jmax (2.0f, radius - 1.3f);
    {
        juce::ColourGradient edge (juce::Colour (0xffe6eef6), 0.0f, face.getY(),
                                   juce::Colour (0xff4f545a), 0.0f, face.getBottom(), false);
        edge.addColour (0.5, juce::Colour (0xff9aabbd));
        g.setGradientFill (edge);
        g.fillRoundedRectangle (face, faceRad);
    }
    const auto body = face.reduced (1.1f);
    juce::ColourGradient faceFill (top, 0.0f, body.getY(),
                                    bottom, 0.0f, body.getBottom(), false);
    faceFill.addColour (0.43, middle);
    g.setGradientFill (faceFill);
    g.fillRoundedRectangle (body, juce::jmax (1.5f, faceRad - 1.2f));
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (body, juce::jmax (1.5f, faceRad - 1.2f));
        g.reduceClipRegion (clip);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRect (body.getX() + 2.0f, body.getY() + 0.6f, body.getWidth() - 4.0f, 1.0f);
        juce::ColourGradient frost (juce::Colours::white.withAlpha (0.18f),
                                    0.0f, body.getY() + 1.6f,
                                    juce::Colours::transparentWhite,
                                    0.0f, body.getY() + body.getHeight() * 0.34f, false);
        g.setGradientFill (frost);
        g.fillRoundedRectangle (body.reduced (0.6f),
                                juce::jmax (1.2f, faceRad - 1.8f));
        juce::ColourGradient skirt (juce::Colours::transparentBlack,
                                    0.0f, body.getBottom() - 4.0f,
                                    juce::Colours::black.withAlpha (0.16f),
                                    0.0f, body.getBottom(), false);
        g.setGradientFill (skirt);
        g.fillRect (body.getX(), body.getBottom() - 4.0f, body.getWidth(), 4.0f);
    }
    g.setColour (juce::Colour (0xff2e2b26).withAlpha (0.80f));
    g.drawRoundedRectangle (face.reduced (0.35f),
                            juce::jmax (2.0f, radius - 1.5f), 0.9f);
    g.setColour (juce::Colours::white.withAlpha (isActive ? 0.40f : 0.06f));
    g.drawRoundedRectangle (face.reduced (1.05f),
                            juce::jmax (1.6f, radius - 2.1f), 0.55f);
}
inline void drawIvoryWell (juce::Graphics& g, juce::Rectangle<float> r,
                           float radius, bool isActive, const Theme& t)
{
    drawFrostedGlassControl (g, r, radius, isActive, t);
}
inline void drawMutedBoneReadout (juce::Graphics& g, juce::Rectangle<float> r,
                                  float radius, bool isActive, const Theme& t)
{
    drawFrostedGlassControl (g, r, radius, isActive, t);
}

inline void drawEmuSpinner (juce::Graphics& g, juce::Rectangle<float> box,
                            bool enabled, const Theme& t)
{
    juce::ignoreUnused (t);
    g.setColour (juce::Colour (0xff2a2722).withAlpha (0.22f));
    g.drawLine (box.getX(), box.getY() + 2.5f, box.getX(), box.getBottom() - 2.5f, 0.8f);
    g.setColour (juce::Colour (0xff2a2722).withAlpha (enabled ? 0.80f : 0.25f));
    const float cx = box.getCentreX();
    const float cy = box.getCentreY();
    juce::Path up, dn;
    up.addTriangle (cx - 2.6f, cy - 1.8f, cx + 2.6f, cy - 1.8f, cx, cy - 5.4f);
    dn.addTriangle (cx - 2.6f, cy + 1.8f, cx + 2.6f, cy + 1.8f, cx, cy + 5.4f);
    g.fillPath (up);
    g.fillPath (dn);
}
inline void drawBayCaption (juce::Graphics& g, juce::Rectangle<float> r,
                            const juce::String& text, const Theme& t)
{
    g.setFont (t.smallLabel (true));
    g.setColour (t.labelInk());
    g.drawFittedText (text.toUpperCase(), r.toNearestInt(), juce::Justification::centred, 1);
}

inline void drawEngravedText (juce::Graphics& g, const juce::String& text,
                              juce::Rectangle<int> area, juce::Justification just,
                              juce::Colour ink, float catchAlpha = 0.45f,
                              juce::Colour catchColour = juce::Colour (0xffe7dec9),
                              bool extraWeight = false)
{
    const auto boneWhite = catchColour;
    const auto edge = boneWhite.withAlpha (catchAlpha);
    const auto soft = boneWhite.withAlpha (catchAlpha * 0.18f);
    g.setColour (soft);
    g.drawFittedText (text, area.translated (-2, 0), just, 1);
    g.drawFittedText (text, area.translated ( 2, 0), just, 1);
    g.drawFittedText (text, area.translated (0, -2), just, 1);
    g.drawFittedText (text, area.translated (0,  2), just, 1);
    g.setColour (edge);
    g.drawFittedText (text, area.translated (0, -1), just, 1);
    g.drawFittedText (text, area.translated (0,  1), just, 1);
    g.setColour (boneWhite.withAlpha (catchAlpha * 0.72f));
    g.drawFittedText (text, area.translated (-1, 0), just, 1);
    g.drawFittedText (text, area.translated ( 1, 0), just, 1);
    g.setColour (ink);
    g.drawFittedText (text, area, just, 1);
    if (extraWeight)
        g.drawFittedText (text, area.translated (1, 0), just, 1);
}
inline void drawCrispText (juce::Graphics& g, juce::Rectangle<float> b, const juce::String& text,
                           float fontSize, juce::Colour colour, bool emphasis = false)
{
    g.setFont (displayFont (fontSize, emphasis));
    g.setColour (colour);
    g.drawText (text, b.toNearestInt(), juce::Justification::centred, false);
}
}
