#pragma once
#include "../UiLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdlib>
namespace trench::ui
{
inline constexpr int   kEditorWidth        = 310;
inline constexpr int   kEditorHeight       = 506;
inline constexpr int   kFaceLockedWidth    = kEditorWidth;
inline constexpr int   kFaceLockedHeight   = kEditorHeight;
inline constexpr int   kEditorHeightClosed = 361;

inline constexpr int   kBayValueWidth      = 46;
inline constexpr int   kBaySourceWidth     = 116;
inline constexpr int   kBaySourceHeight    = 20;

inline constexpr float kLabelPt            = 12.0f;
inline constexpr float kValuePt            = 14.0f;
inline constexpr int   kRowH               = 26;
inline constexpr float kBayValuePt         = kValuePt;
inline constexpr int   kBayValueHeight     = 17;

inline constexpr float kBayKnobDiameter    = 36.0f;
inline constexpr float kPanelSourceWidth   = 1024.0f;
inline constexpr float kPanelSourceHeight  = 1536.0f;
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
inline constexpr float kMicroCaptionPt = 10.0f;
inline juce::Font telemetryFont (float height, bool emphasis = false)
{

    return displayFont (height, emphasis);
}
struct Theme
{
    const trench::UiLayout& layout;
    juce::Colour accent()      const { return layout.colour ("accent",      juce::Colour (0xff3cc8be)); }
    juce::Colour curveColour() const { return layout.colour ("curveColour", juce::Colour (0xff5ae0a9)); }
    juce::Colour curveHighlight() const { return layout.colour ("curveHighlight", juce::Colour (0xffb2fadb)); }
    juce::Colour telemetry() const { return layout.colour ("telemetry", juce::Colour (0xff608074)); }
    juce::Colour rollerIllumination() const { return layout.colour ("rollerIllumination", juce::Colour (0xff5ae0a9)); }
    juce::Colour modulationLamp() const { return layout.colour ("modulationLamp", juce::Colour (0xff5ae0a9)); }
    juce::Colour glassTop()    const { return layout.colour ("glassTop",    juce::Colour (0xff080a06)); }
    juce::Colour glassBottom() const { return layout.colour ("glassBottom", juce::Colour (0xff030402)); }
    juce::Colour gridTint()    const { return layout.colour ("gridTint",    juce::Colour (0xff2a5c48)); }
    juce::Colour amber()       const { return layout.colour ("amber",       juce::Colour (0xffa9554e)); }
    juce::Colour wellTop()     const { return layout.colour ("wellTop",     juce::Colour (0xffcbd8e6)); }
    juce::Colour wellBottom()  const { return layout.colour ("wellBottom",  juce::Colour (0xff9db0c4)); }
    juce::Colour wellKeyline() const { return layout.colour ("wellKeyline", juce::Colour (0xffe6eef6)); }
    juce::Colour bevelHi()     const { return layout.colour ("bevelHi",     juce::Colour (0x88e7dec9)); }
    juce::Colour bevelLo()     const { return layout.colour ("bevelLo",     juce::Colour (0x3d000000)); }
    juce::Colour arrow()       const { return layout.colour ("arrow",       juce::Colour (0xff241e15)); }
    juce::Colour rim()         const { return layout.colour ("rim",         juce::Colour (0xff241e15)); }
    juce::Colour screenEdge()  const { return layout.colour ("screenEdge",  juce::Colour (0xff171325)); }
    juce::Colour labelInk()    const { return layout.colour ("labelInk",    juce::Colour (0xff24231f)); }
    juce::Colour phosphor()    const { return layout.colour ("phosphor",    juce::Colour (0xff1a1624)); }
    juce::Colour spectrumGhost() const { return layout.colour ("spectrumGhost", juce::Colour (0xff9fcfc4)); }
    float  wellRadius()        const { return (float) layout.param ("wellRadius", 9.0); }
    float  gridBoost()         const { return (float) layout.param ("gridBoost", 6.0); }
    double themeParam (const juce::String& name, double fallback) const { return layout.param (name, fallback); }
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
        constexpr float fontReferenceWidth = 360.0f;
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
    const auto warmth = juce::Colour (0xffe7edf5);
    const auto top    = juce::Colour (0xffc9d6e2).interpolatedWith (warmth, 0.10f);
    const auto middle = juce::Colour (0xffb2c4d6).interpolatedWith (warmth, 0.08f);
    const auto bottom = juce::Colour (0xff94a8c0).interpolatedWith (warmth, 0.06f);
    const float faceRad = juce::jmax (2.0f, radius - 1.3f);
    {
        juce::ColourGradient edge (juce::Colour (0xffe9eff5), 0.0f, face.getY(),
                                   juce::Colour (0xff5f6e7e), 0.0f, face.getBottom(), false);
        edge.addColour (0.5, juce::Colour (0xffa6b4c4));
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
    g.setColour (isActive ? t.accent().withAlpha (0.36f)
                          : juce::Colours::white.withAlpha (0.06f));
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
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawFittedText (text.toUpperCase(), r.toNearestInt().translated (0, 1), juce::Justification::centred, 1);
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

inline constexpr juce::uint32 kSheetField = 0xfffcfcfd, kSheetFieldLow = 0xfff1f2f3, kSheetBorder = 0xff5a5750,
                              kSheetInk = 0xff2a2722, kSheetInkDim = 0xff7a766e, kSheetHighlight = 0xff3cc8be,
                              kSheetRule = 0xffd6d7d9;
inline constexpr float kSheetRadius = 3.0f, kSheetRowH = 22.0f, kSheetGutter = 18.0f, kSheetPadRight = 14.0f,
                       kSheetRowPt = 13.0f, kSheetHeadingPt = 10.0f;
inline juce::Font sheetRowFont (bool emphasis = false) { return displayFont (kSheetRowPt, emphasis); }
inline juce::Font sheetHeadingFont() { return displayFont (kSheetHeadingPt, true); }
inline juce::Font sheetDetailFont() { return displayFont (kSheetRowPt - 1.5f, false); }
inline void drawSheet (juce::Graphics& g, juce::Rectangle<float> r, int shadow = 10)
{
    juce::Path outline;
    outline.addRoundedRectangle (r, kSheetRadius);
    juce::DropShadow (juce::Colours::black.withAlpha (0.28f), shadow, { 0, shadow / 3 }).drawForPath (g, outline);
    juce::DropShadow (juce::Colours::black.withAlpha (0.18f), 2, { 0, 1 }).drawForPath (g, outline);
    juce::ColourGradient field (juce::Colour (kSheetField), 0.0f, r.getY(),
                                juce::Colour (kSheetFieldLow), 0.0f, r.getBottom(), false);
    g.setGradientFill (field);
    g.fillPath (outline);
    g.setColour (juce::Colour (kSheetBorder).withAlpha (0.9f));
    g.drawRoundedRectangle (r.reduced (0.5f), kSheetRadius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawLine (r.getX() + kSheetRadius, r.getY() + 1.5f, r.getRight() - kSheetRadius, r.getY() + 1.5f, 1.0f);
}
inline void drawSheetHeading (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& text,
                              const juce::String& detail = {})
{
    g.setFont (sheetHeadingFont());
    g.setColour (juce::Colour (kSheetInkDim));
    auto label = cell.withTrimmedLeft (kSheetGutter).withTrimmedRight (kSheetPadRight);
    g.drawText (text.toUpperCase(), label.toNearestInt(), juce::Justification::centredLeft, false);
    if (detail.isNotEmpty())
    {
        g.drawText (detail.toUpperCase(), label.toNearestInt(), juce::Justification::centredRight, false);
        label.removeFromRight (juce::GlyphArrangement::getStringWidth (sheetHeadingFont(), detail.toUpperCase()) + 8.0f);
    }
    const float w = juce::GlyphArrangement::getStringWidth (sheetHeadingFont(), text.toUpperCase());
    g.setColour (juce::Colour (kSheetRule));
    g.fillRect (label.getX() + w + 8.0f, cell.getCentreY(), juce::jmax (0.0f, label.getRight() - (label.getX() + w + 8.0f)), 1.0f);
}
inline void drawSheetRow (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& text,
                          bool hot, bool ticked, bool enabled = true, const juce::String& detail = {})
{
    if (hot && enabled)
    {
        g.setColour (juce::Colour (kSheetHighlight).withAlpha (0.16f));
        g.fillRect (cell.reduced (1.0f, 0.0f));
        g.setColour (juce::Colour (kSheetHighlight).withAlpha (0.9f));
        g.fillRect (cell.withWidth (2.0f).translated (1.0f, 0.0f));
    }
    if (ticked)
    {
        juce::Path check;
        const float cx = cell.getX() + 9.0f, cy = cell.getCentreY();
        check.startNewSubPath (cx - 3.0f, cy);
        check.lineTo (cx - 1.0f, cy + 2.5f);
        check.lineTo (cx + 3.5f, cy - 3.0f);
        g.setColour (juce::Colour (kSheetInk));
        g.strokePath (check, { 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }
    auto label = cell.withTrimmedLeft (kSheetGutter).withTrimmedRight (kSheetPadRight);
    if (detail.isNotEmpty())
    {
        g.setFont (sheetDetailFont());
        g.setColour (juce::Colour (kSheetInkDim));
        g.drawText (detail, label.toNearestInt(), juce::Justification::centredRight, false);
        label.removeFromRight (juce::GlyphArrangement::getStringWidth (sheetDetailFont(), detail) + 14.0f);
    }
    g.setFont (sheetRowFont (ticked));
    g.setColour (juce::Colour (enabled ? kSheetInk : kSheetInkDim));
    g.drawText (text, label.toNearestInt(), juce::Justification::centredLeft, false);
}
}
