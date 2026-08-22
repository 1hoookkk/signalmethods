#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <map>
#include <optional>
#include <vector>
namespace trench
{
struct UiElementLayout
{
    juce::Rectangle<float> sourceRect;
    std::optional<float> fontSize;
    std::optional<juce::Colour> textColour;
    std::optional<juce::String> text;
    std::optional<float> opacity;
};
struct Decal
{
    juce::String type;
    juce::Rectangle<float> sourceRect;
    juce::String text;
    juce::Colour colour { juce::Colours::white };
    float fontSize = 12.0f;
    float thickness = 1.5f;
    bool fill = false;
};
class UiLayout
{
public:
    static UiLayout defaults()
    {
        UiLayout layout;
        // ONE well geometry for both wheels, on the DISPLAY's own left edge
        // (source x 110): the two wells used to differ by a pixel each way.
        // Sized to HOLD the 148x33 frame without clipping its rounded caps, and
        // centred on the apertures measured off the plate art (morph centre
        // y 233.62, q centre y 287.26, both x 106.70 in face px).
        // The E-mu reference is a shallow, fully visible roller: matte ribs and
        // a small transmitted-light packet, not a large drum cropped by the
        // aperture. These are the earlier clean-face bounds restored verbatim.
        // RE-MEASURED off the definitive master (Tyson 2026-08-11: "re measure
        // the actual well, the wheel is slightly too small"). The old numbers
        // were measured against the previous plate and the drum sat inside its
        // opening rather than filling it. Apertures found by thresholding the
        // 828x1280 plate at luminance 55 and scaled by 1010/828, 1557/1280:
        //   MORPH  plate x97 y561 w349 h66
        //   Q      plate x97 y697 w349 h69   (the master's Q slot IS 3px taller)
        // WHEEL GEOMETRY IS NOT TO BE "CORRECTED" TO THE STRIP'S ASPECT.
        // Tried 2026-08-12 and reverted the same pass: the wheel sits BEHIND the
        // plate and is only ever seen through the punched hole, which measures
        // 131.3 x 20.0 editor px (aspect 6.5). The component is therefore
        // deliberately LARGER than what shows - matching it to the strip's own
        // 4.4839 made the drum taller than the opening and changed how much of
        // the barrel reads through it (Tyson: "looks worse and dont sit
        // properly"). The aperture is not the visible area. These are the
        // judged numbers; leave them.
        // READOUT PLACEMENT follows the X3 proportion (Tyson 2026-08-12): the
        // value box sits hard against its roller - a 6px gap, not 13.4 - and
        // slightly BELOW the roller's centre line rather than on it. Dead
        // centring is a modern habit; the reference hangs the number low.
        // THE RECT IS THE VISIBLE WHEEL (2026-08-13). It used to be sized to the
        // filmstrip FRAME, padding and all, and WheelControl filled it - so the
        // wheel that actually showed was 3% smaller than the rect, seated off
        // its own centre, and the two wheels were DIFFERENT SIZES because the
        // two rects were (97.7 against 102.3, the master's Q slot being 3px
        // taller). One wheel, one size, seated on each opening's own centre.
        // Measured off the plate art: both openings are x 97..445 plate px, the
        // MORPH slot y 562..626 and the Q slot y 698..765, which is 137.4 x 25.6
        // and 137.4 x 26.7 in editor px. The wheel is 127.5 x 25.5 - the ink
        // aspect is 405:81, so height fills the slot and the width that follows
        // leaves ~5px of floor at each cap (the flat-pitchwheel overscan now
        // covers it; WheelControl::kThrowOverscan).
        // 139x31 editor pixels at the 1010x1557 source scale, centred on the
        // same measured well axes. This restores the locked native-frame law.
        layout.elements["morphWheel"]   = { { 115.9f, 675.0f, 430.6f, 96.0f }, {}, {} };
        layout.elements["qWheel"]       = { { 115.9f, 842.5f, 430.6f, 96.0f }, {}, {} };
        layout.elements["typeSelector"] = { { 230.0f, 139.0f, 675.0f, 68.0f },  {}, {} };
        // The bay's two door-words (SectionRail seat, PluginEditor
        // kBaySelector {43.5, 328, 114, 17} editor px at the 326/1010 map).
        // Stated here so the onboarding tour can spotlight it.
        layout.elements["bayDoors"]     = { { 134.8f, 1016.2f, 353.2f, 52.7f }, {}, {} };
        // The panel asset contains readout recesses, but they are not part of
        // the desired control grammar. The white modules therefore cover the
        // recesses completely. Both use the same 2.5:1 E-mu proportion and the
        // geometry measured from the selected clean-face reference.
        // Ink is TRUE BLACK, not the plate's 0x222222 (Tyson: "make the text
        // more readable at small screens"). Measured on the render, these digits
        // were the lowest-contrast text on the whole face at 0.40, against
        // 0.55-0.58 for the engraved plate labels - a light frosted panel needs
        // a darker ink than beige does, and the reference readout's digits are
        // pure black.
        // E-MU MEASURED, every number (Tyson 2026-08-09: "Reference the emu
        // proportions and typography... the readout close to the wheels").
        // Off BITMAP4615/4602: pill = 42.5% of wheel width at 2.85:1 ->
        // 182x64 for the 428 wheels; pill centre sits 12% of wheel height
        // BELOW the wheel centre (their 3px of 25) -> y 705 / 873.5; digit
        // cap = 54% of pill height, 1px stems -> fontSize 21, regular.
        // The v2 plate has no readout recesses; the pills float on plate.
        layout.elements["morphReadout"] = { { 562.6f, 698.0f, 167.3f, 68.1f },  18.0f, juce::Colour (0xff3A2A0E) };
        layout.elements["qReadout"]     = { { 562.6f, 865.2f, 167.3f, 68.1f },  18.0f, juce::Colour (0xff3A2A0E) };
        // RE-MEASURED with per-axis mapping (2026-08-15 "Re measure all
        // bounds"; plate->source is x*1010/828, y*1557/1280 - NOT one factor).
        // The recess interior in df2_panel_beige.png (luma < 60) is plate px
        // x 97..738, y 191..498 -> source x 118.3..901.4, y 232.3..607.0.
        // The wheel wells were measured the same pass and their elements sit
        // dead-centred on the punched holes (centres agree to 0.05 px).
        layout.elements["spectrumGrid"] = { { 118.3f, 232.3f, 783.1f, 374.7f }, {}, {} };
        // anchored near its box so the eye tracks the pair (UX audit 2026-07-31)
        // the ROW starts where the display starts (source x 110): the label is
        // set flush left there, the selector still ends on the display's right
        layout.elements["typeLabel"]    = { { 152.0f, 139.0f, 72.0f, 68.0f },  15.0f, juce::Colour (0xff222222) };
        layout.elements["typeLabel"].text = "BODY";
        // Black, like the readouts: text on the light frosted panel needs a
        // darker ink than text engraved into beige (contrast measured 0.43 here
        // against 0.55-0.58 on the plate).
        layout.elements["typeName"]     = { { 244.0f, 143.0f, 530.0f, 64.0f },  18.0f, juce::Colour (0xff3A2A0E) };
        layout.elements["typeArrow"]    = { { 850.0f, 143.0f, 52.0f,  64.0f },  {}, {} };
        layout.elements["morphLabel"]   = { { 114.0f, 640.0f, 434.0f, 38.0f },  18.0f, juce::Colour (0xff222222) };
        layout.elements["morphLabel"].text = "MORPH";
        layout.elements["qLabel"]       = { { 114.0f, 798.0f, 434.0f, 38.0f },  18.0f, juce::Colour (0xff222222) };
        layout.elements["qLabel"].text = "Q";
        // No per-element ink: the logo takes the brandInk palette token, so a
        // theme swap carries it. Every other label stays plate near-black.
        layout.elements["brandLabel"]   = { { 100.0f, 73.0f, 230.0f, 44.0f }, 20.0f, juce::Colour (0xff0f0c09) };
        // The name is TRENCH (Tyson 2026-08-09, final): detached, slightly
        // unknowable - never trench imagery, military language or depth
        // claims anywhere on the product.
        layout.elements["brandLabel"].text = "TRENCH";
        // MIX is optically centred in the gutter between the readouts' right
        // edge and the inner chassis boundary - it used to sit 8px too far right
        // Centred over the DRUM's own column (Tyson 2026-08-10), not the
        // oversized grab element: the drum centre sits at source x 902.5.
        // DRIER, SMALLER (Tyson 2026-08-12): MORPH %, Q % and MIX were the
        // editorial treatment - 17/16 in the 440 reference space put them at
        // 12.6/11.9 real px, competing with the readouts they caption. Dropped
        // to a technical caption size; the wheels and the numbers lead.
        layout.elements["amountLabel"] = { { 831.6f, 656.0f, 110.0f, 30.0f },
                                             17.0f, juce::Colour (0xff222222) };
        layout.elements["amountLabel"].text = "MIX";
        // Wide enough to hold what the wheel PAINTS, not just the drum: the
        // drum is 12 screen px and its seat shadow reaches another 12 to the
        // right. At the old 60 the component was 21 screen px and guillotined
        // the shadow at 62% opacity - a hard vertical step that made the
        // shadow read as a separate slab instead of the drum's own dark side
        // (Tyson 2026-08-09: "reads disconnected... it's a wheel edge").
        // 95 x 217 gives 32 x 71: drum + full shadow + a little grab air, with
        // the drum landing on exactly the same pixel column as before.
        layout.elements["amountWheel"] = { { 855.0f, 700.0f, 95.0f, 217.0f }, {}, {} };
        // MIX was the only control on the plate that never said its value —
        // MORPH, Q and every bay lane state theirs. E-MU pairs each roller with
        // a readout too (BITMAP4615 shows two of them). Sits under the rail on
        // the rail's own centre line, in the same family as the primaries.
        // ARCTIC ICE (selected board, 2026-08-09). Four restrained roles:
        // smoked slate glass, pale curve, frosted internal wheel light, dusty
        // telemetry. The plate and white readout hardware remain unchanged.
        // ONE MINT, FOUR STATIONS (Tyson 2026-08-12). The hue runs across the
        // face at four different intensities, brightest where the light is
        // physically hottest:
        //   wheel hotspot   #DDF3E7  pale, near-white mint
        //   wheel falloff   #6FC7A2  saturated mineral mint
        //   response curve  #121410  black - the trace is ink on the panel
        //   grid            #9AA28B  pale olive, LIFTING off the panel
        // The curve is deliberately the quiet one: on a pale panel it is ink
        // in the family, not a lamp.
        // The wheel lamp follows the CURVE's hue (Tyson 2026-08-11: "the lamp
        // is too desaturated, it should follow the curve colour"). Same hue,
        // two states: ink on the panel, light in the drum. Saturation pulled
        // back from #7CC23A on 2026-08-12 ("the green sticks out") - once the
        // plate went stone, a leaf green was the only vivid thing on a face
        // that had gone quiet everywhere else.
        // POSITIVE LCD (Tyson 2026-08-11). The panel is pale grey-green and
        // EVERY mark on it is dark - the trace is ink, not light. The only
        // emissive thing left on the face is the wheel packet, which is what a
        // real unit does: the display is passive, the lamps are not.
        layout.colours["accent"]             = juce::Colour (0xffEC3C1C);
        layout.colours["curveColour"]        = juce::Colour (0xffEC3C1C);
        layout.colours["curveHighlight"]     = juce::Colour (0xffF04120);
        // READOUT LOCK (Tyson 2026-08-06): readouts/telemetry are a FIXED ink -
        // they never take the curve's or the glass's hue.
        layout.colours["telemetry"]          = juce::Colour (0xff8A93A2);  // fixed readout ink
        layout.colours["rollerIllumination"] = juce::Colour (0xffEC3C1C);
        layout.colours["modulationLamp"]     = juce::Colour (0xffEC3C1C);
        layout.colours["phosphor"]           = juce::Colour (0xff202A40);  // dark cool glass
        // amber = the SOURCE voice (RESAMPLE/GEN only) - never the trace's value
        layout.colours["amber"]              = juce::Colour (0xffb8862e);
        layout.colours["screenEdge"]         = juce::Colour (0xff0C110E);  // near-black frame
        // No glow on the curve: the ghost halo matches the glass exactly, so
        // the trace reads as a clean line with no aura.
        layout.colours["spectrumGhost"]      = juce::Colour (0xff202A40);
        layout.colours["labelInk"]    = juce::Colour (0xff222222);
        // The TRENCH title takes no token of its own: it is the window's navy
        // (phosphor) lettering inside a catch of the trace's mint (accent),
        // both already defined above. See LabelsLayer.
        // WELL INSERT LAW (Tyson 2026-08-04): the recessed well reads as a
        // separate dark-metal insert bolted into the beige plate - dark
        // charcoal floor (#2A2A2A) framing the matte-black wheels.
        layout.colours["wellTop"]            = juce::Colour (0xff2A2A2A);
        layout.colours["wellBottom"]         = juce::Colour (0xff1A1A1A);
        layout.colours["wellKeyline"]        = juce::Colour (0xff4A4A4A);
        layout.params["wellRadius"]        = 9.0;
        layout.params["readoutAliasScale"] = 0.95;
        layout.params["typeArrowExtra"]    = 6.0;
        layout.params["curveDbTop"]        = 40.0;
        layout.params["curveDbBottom"]     = -40.0;
        layout.params["fontBold"]          = 1.0;
        layout.strings["fontFamily"] = "Arial";
        layout.strings["fontFamilyEmphasis"] = "Arial";
        return layout;
    }
    static UiLayout fromJson (const juce::String& jsonText)
    {
        UiLayout layout = defaults();
        const juce::var root = juce::JSON::parse (jsonText);
        auto* obj = root.getDynamicObject();
        if (obj == nullptr)
            return layout;
        auto hex = [] (const juce::var& v)
        { return juce::Colour ((juce::uint32) v.toString().getHexValue32()); };
        if (auto* els = obj->getProperty ("elements").getDynamicObject())
            for (auto& p : els->getProperties())
            {
                auto& el = layout.elements[p.name.toString()];
                if (auto* eo = p.value.getDynamicObject())
                {
                    if (auto* r = eo->getProperty ("rect").getArray(); r != nullptr && r->size() == 4)
                        el.sourceRect = { (float) (double) (*r)[0], (float) (double) (*r)[1],
                                          (float) (double) (*r)[2], (float) (double) (*r)[3] };
                    if (eo->hasProperty ("fontSize"))
                        el.fontSize = (float) (double) eo->getProperty ("fontSize");
                    if (eo->hasProperty ("textColor"))
                        el.textColour = hex (eo->getProperty ("textColor"));
                    if (eo->hasProperty ("text"))
                        el.text = eo->getProperty ("text").toString();
                    if (eo->hasProperty ("opacity"))
                        el.opacity = (float) (double) eo->getProperty ("opacity");
                }
            }
        if (auto* cols = obj->getProperty ("colours").getDynamicObject())
            for (auto& p : cols->getProperties())
                layout.colours[p.name.toString()] = hex (p.value);
        if (auto* pars = obj->getProperty ("params").getDynamicObject())
            for (auto& p : pars->getProperties())
                layout.params[p.name.toString()] = (double) p.value;
        if (auto* strs = obj->getProperty ("strings").getDynamicObject())
            for (auto& p : strs->getProperties())
                layout.strings[p.name.toString()] = p.value.toString();
        if (auto* arr = obj->getProperty ("decals").getArray())
        {
            layout.decals.clear();
            for (auto& dv : *arr)
                if (auto* d = dv.getDynamicObject())
                {
                    Decal dec;
                    dec.type = d->getProperty ("type").toString();
                    if (auto* r = d->getProperty ("rect").getArray(); r != nullptr && r->size() == 4)
                        dec.sourceRect = { (float) (double) (*r)[0], (float) (double) (*r)[1],
                                           (float) (double) (*r)[2], (float) (double) (*r)[3] };
                    dec.text = d->getProperty ("text").toString();
                    if (d->hasProperty ("color"))     dec.colour = hex (d->getProperty ("color"));
                    if (d->hasProperty ("colour"))    dec.colour = hex (d->getProperty ("colour"));
                    if (d->hasProperty ("fontSize"))  dec.fontSize  = (float) (double) d->getProperty ("fontSize");
                    if (d->hasProperty ("thickness")) dec.thickness = (float) (double) d->getProperty ("thickness");
                    if (d->hasProperty ("fill"))      dec.fill = (bool) d->getProperty ("fill");
                    layout.decals.push_back (dec);
                }
        }
        return layout;
    }
    juce::String toJson() const
    {
        auto* root = new juce::DynamicObject();
        auto* els = new juce::DynamicObject();
        for (const auto& e : elements)
        {
            auto* eo = new juce::DynamicObject();
            juce::Array<juce::var> rect;
            rect.add (e.second.sourceRect.getX());     rect.add (e.second.sourceRect.getY());
            rect.add (e.second.sourceRect.getWidth()); rect.add (e.second.sourceRect.getHeight());
            eo->setProperty ("rect", rect);
            if (e.second.fontSize)   eo->setProperty ("fontSize", *e.second.fontSize);
            if (e.second.textColour) eo->setProperty ("textColor", juce::String::toHexString ((int) e.second.textColour->getARGB()));
            if (e.second.text)       eo->setProperty ("text", *e.second.text);
            if (e.second.opacity)    eo->setProperty ("opacity", *e.second.opacity);
            els->setProperty (e.first, juce::var (eo));
        }
        root->setProperty ("elements", juce::var (els));
        auto* cols = new juce::DynamicObject();
        for (const auto& c : colours)
            cols->setProperty (c.first, juce::String::toHexString ((int) c.second.getARGB()));
        root->setProperty ("colours", juce::var (cols));
        auto* pars = new juce::DynamicObject();
        for (const auto& p : params)
            pars->setProperty (p.first, p.second);
        root->setProperty ("params", juce::var (pars));
        auto* strs = new juce::DynamicObject();
        for (const auto& s : strings)
            strs->setProperty (s.first, s.second);
        root->setProperty ("strings", juce::var (strs));
        return juce::JSON::toString (juce::var (root), false);
    }
    juce::Rectangle<float> sourceRectFor (const juce::String& id,
                                          juce::Rectangle<float> fallback = {}) const
    {
        const auto it = elements.find (id);
        return it != elements.end() ? it->second.sourceRect : fallback;
    }
    std::optional<float> fontSizeFor (const juce::String& id) const
    {
        const auto it = elements.find (id);
        return it != elements.end() ? it->second.fontSize : std::nullopt;
    }
    std::optional<juce::Colour> textColourFor (const juce::String& id) const
    {
        const auto it = elements.find (id);
        return it != elements.end() ? it->second.textColour : std::nullopt;
    }
    juce::String textFor (const juce::String& id, const juce::String& fallback) const
    {
        const auto it = elements.find (id);
        return it != elements.end() && it->second.text ? *it->second.text : fallback;
    }
    float opacityFor (const juce::String& id) const
    {
        const auto it = elements.find (id);
        return it != elements.end() && it->second.opacity
                   ? juce::jlimit (0.0f, 1.0f, *it->second.opacity) : 1.0f;
    }
    juce::Colour colour (const juce::String& id, juce::Colour fallback) const
    {
        const auto it = colours.find (id);
        return it != colours.end() ? it->second : fallback;
    }
    double param (const juce::String& id, double fallback) const
    {
        const auto it = params.find (id);
        return it != params.end() ? it->second : fallback;
    }
    juce::String string (const juce::String& id, const juce::String& fallback) const
    {
        const auto it = strings.find (id);
        return it != strings.end() && it->second.isNotEmpty() ? it->second : fallback;
    }
    std::map<juce::String, UiElementLayout> elements;
    std::map<juce::String, juce::Colour> colours;
    std::map<juce::String, double> params;
    std::map<juce::String, juce::String> strings;
    std::vector<Decal> decals;
};
}
