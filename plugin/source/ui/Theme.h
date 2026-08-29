#pragma once
#include "../UiLayout.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdlib>
namespace trench::ui
{
inline constexpr int   kEditorWidth        = 326;
inline constexpr int   kEditorHeight       = 503;
inline constexpr int   kEditorHeightClosed = 361;
// The bay's shared value-column width: every room row - knob readouts and the
// SOURCE selector alike - draws the same frosted box on the same axis.
// Bay readouts are the MORPH/Q readouts' smaller SIBLINGS, not a different
// family: the primaries are 61x25 (aspect 2.5), so these hold the same
// aspect at ~75% scale. 86x13 was aspect 6.6 and WIDER than the primaries,
// which inverted the hierarchy (Tyson 2026-08-05: wrong proportions).
inline constexpr int   kBayValueWidth      = 46;   // numeric boxes (352px-face legibility)
inline constexpr int   kBaySourceWidth     = 116;  // the longest preset name plus its menu chevron
inline constexpr int   kBaySourceHeight    = 20;   // a dropdown needs more than a number does
// BAY TYPE, in ACTUAL pixels. The rest of the face sizes type in a 440-wide
// reference space that Theme::fontSize scales by 326/440; the bay used to mix
// the two, so its caption landed at 7.4px while its value landed at 8.5px -
// the label smaller than the number, both under the face's 11-15px range and
// unreadable at the shipped size. One system now: these are real pixels.
inline constexpr float kBayValuePt          = 12.0f;
inline constexpr int   kBayValueHeight     = 17;   // shared by every bay box
// The filmstrip frame draws at d * 96/76 and its content (disc + baked contact
// shadow) fills 80 of those 96px, so a row must be at least 1.053 * d tall or
// one knob's shadow lands on the next. A 32px disc in a 36px row leaves enough
// real air for the compact 326x503 face without letting adjacent shadows touch.
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
    static juce::String* family = new juce::String (kUiFontName);   // leaked on purpose
    return *family;
}
inline juce::String& uiEmphasisFontFamily()
{
    static juce::String* family = new juce::String (kUiEmphasisFontName);   // leaked on purpose
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
inline constexpr float kPlateWordPt    = 9.5f;
inline constexpr float kMicroCaptionPt = 8.6f;
inline juce::Font telemetryFont (float height, bool emphasis = false)
{
    // ONE TYPEFACE: the readouts and panel labels share the compact Windows-era
    // Tahoma voice visible in the E-mu reference. Hierarchy comes from size and
    // weight, not a second display font.
    return displayFont (height, emphasis);
}
struct Theme
{
    const trench::UiLayout& layout;
    juce::Colour accent()      const { return layout.colour ("accent",      juce::Colour (0xffc96a54)); }
    juce::Colour curveColour() const { return layout.colour ("curveColour", juce::Colour (0xffd9d6c9)); }
    juce::Colour curveHighlight() const { return layout.colour ("curveHighlight", juce::Colour (0xffefece2)); }
    juce::Colour telemetry() const { return layout.colour ("telemetry", juce::Colour (0xffa39ac0)); }
    juce::Colour rollerIllumination() const { return layout.colour ("rollerIllumination", juce::Colour (0xffc96a54)); }
    juce::Colour modulationLamp() const { return layout.colour ("modulationLamp", juce::Colour (0xffc96a54)); }
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
    /// The same rect BEFORE the editor mapping, for anything that has to index
    /// back into the plate bitmap's own pixels.
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
    struct EditorDecal
    {
        juce::String type, text;
        juce::Rectangle<float> rect;
        juce::Colour colour;
        float fontSize, thickness;
        bool fill;
    };
    std::vector<EditorDecal> decals() const
    {
        constexpr float s = (float) kEditorHeight / kPanelSourceHeight;
        std::vector<EditorDecal> out;
        out.reserve (layout.decals.size());
        for (const auto& d : layout.decals)
            out.push_back ({ d.type, d.text, sourceRectToEditor (d.sourceRect), d.colour,
                             d.fontSize * s, d.thickness * s, d.fill });
        return out;
    }
};
// RESTORED VERBATIM from 4db42c24 "Face: Five Point construction + the
// actual citron UI" (Tyson 2026-08-12: "get the readouts and stuff back,
// they look better"). This session had warmed it to bone, then cooled it to
// a neutral silver, then swapped its seat for a cut recess - three passes
// away from the construction that was judged good. Recovered from
// C:/Users/hooki/trench-workstation, which still holds this history.
// DO NOT re-tune these stops without a verdict against a render.
// READOUTS from d9bdfe97, the goal build (Tyson 2026-08-12: "the way the
// readouts look ... everything else we want"). Verbatim.
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
// The bay's caption: the SAME voice as the anchor labels (verdict 2026-08-01
// "it looks disconnected") - same family, same tracking, plain ink; only the
// size steps down. No private engraving treatment.
// THE E-MU SPINNER (Tyson 2026-08-09: "add a up down on the rooms. And the
// preset and room selector arent the same"): the X3 sheet's stacked up/down
// stepper pair, one construction for every choice box that steps. Same
// triangle geometry as the readouts' adjust cue - one family, stated once.
// Hit-testing is the caller's, split at the box's vertical centre.
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
// The catch defaults to the plate's own bone white - an engraving lit from
// above. A caller can hand it a colour instead, which turns the same passes
// into E-MU's panel-title glow: measured on their FILTER, dark letterforms
// inside a ~2px halo that peaks at the raw accent (#6CDBDA, sat 0.51 val 0.86)
// and is back to panel by the third pixel out.
// extraWeight draws the ink twice, a pixel apart, which widens every stem by
// one pixel with the letterforms untouched. JUCE's Font carries bold as a style
// flag, not a numeric weight axis, so above Arial Bold this is the only way to
// add weight without changing typeface - and the ask was heavier type, not
// different type.
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
