#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <cstdlib>
#include <map>
#include <optional>
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
class UiLayout
{
public:
    static UiLayout defaults()
    {
        UiLayout layout;

        layout.elements["morphWheel"]   = { { 106.2f, 675.1f, 442.8f, 99.7f }, {}, {} };
        layout.elements["qWheel"]       = { { 106.2f, 842.4f, 442.8f, 99.7f }, {}, {} };
        layout.elements["typeSelector"] = { { 230.0f, 139.0f, 666.6f, 68.0f },  {}, {} };

        layout.elements["keyBox"]       = { { 551.6f, 65.0f, 345.0f, 62.0f }, {}, {} };
        layout.elements["gainLabel"]    = { { 149.2f, 1015.0f, 126.0f, 49.0f }, 12.5f, juce::Colour (0xff2a2722) };
        layout.elements["gainLabel"].text = "GAIN";

        layout.elements["morphReadout"] = { { 588.0f, 694.0f, 172.0f, 62.0f },  16.0f, juce::Colour (0xff3f3a33) };
        layout.elements["qReadout"]     = { { 588.0f, 861.2f, 172.0f, 62.0f },  16.0f, juce::Colour (0xff3f3a33) };

        layout.elements["spectrumGrid"] = { { 120.7f, 238.8f, 769.9f, 361.7f }, {}, {} };

        layout.elements["typeLabel"]    = { { 126.9f, 139.0f, 97.0f, 68.0f },  14.5f, juce::Colour (0xff2a2722) };
        layout.elements["typeLabel"].text = "BODY";

        layout.elements["typeName"]     = { { 244.0f, 143.0f, 522.0f, 64.0f },  20.0f, juce::Colour (0xff2a2722) };
        layout.elements["typeArrow"]    = { { 841.6f, 143.0f, 52.0f,  64.0f },  {}, {} };
        layout.elements["morphLabel"]   = { { 109.8f, 635.1f, 442.8f, 38.0f },  17.0f, juce::Colour (0xff2a2722) };
        layout.elements["morphLabel"].text = "MORPH";
        layout.elements["qLabel"]       = { { 109.8f, 802.35f, 442.8f, 38.0f },  17.0f, juce::Colour (0xff2a2722) };
        layout.elements["qLabel"].text = "Q";

        layout.elements["brandLabel"]   = { { 126.9f, 66.0f, 300.0f, 56.0f }, 29.0f, juce::Colour (0xff0f0c09) };

        layout.elements["brandLabel"].text = "TRENCH";
        layout.colours["accent"]             = juce::Colour (0xff3cc8be);
        layout.colours["curveColour"]        = juce::Colour (0xffbef0d7);
        layout.colours["curveHighlight"]     = juce::Colour (0xffe6fff4);
        layout.colours["telemetry"]          = juce::Colour (0xff608074);
        layout.colours["dashed"]             = juce::Colour (0xff608074);
        layout.colours["rollerIllumination"] = juce::Colour (0xff3cc8be);
        layout.colours["modulationLamp"]     = juce::Colour (0xff3cc8be);
        layout.colours["phosphor"]           = juce::Colour (0xff1a1624);

        layout.colours["amber"]              = juce::Colour (0xffb8862e);
        layout.colours["screenEdge"]         = juce::Colour (0xff171325);

        layout.colours["spectrumGhost"]      = juce::Colour (0xff1a1624);
        layout.colours["labelInk"]    = juce::Colour (0xff24231f);

        layout.colours["wellTop"]            = juce::Colour (0xff2A2A2A);
        layout.colours["wellBottom"]         = juce::Colour (0xff1A1A1A);
        layout.colours["wellKeyline"]        = juce::Colour (0xff4A4A4A);
        layout.params["wellRadius"]        = 9.0;
        layout.params["readoutAliasScale"] = 0.95;
        layout.params["typeArrowExtra"]    = 6.0;
        layout.params["curveDbTop"]        = 40.0;
        layout.params["curveDbBottom"]     = -40.0;
        layout.params["fontBold"]          = 1.0;
        layout.params["gridBoost"]         = 3.0;
        if (const char* themeName = std::getenv ("TRENCH_THEME"))
        {
            if (juce::String (themeName).toLowerCase() == "methods")
            {
                const juce::Colour mint (0xff3cc8be);
                const juce::Colour bone (0xffe7dec9);
                layout.colours["glassTop"]           = juce::Colour (0xff050506);
                layout.colours["glassBottom"]        = juce::Colour (0xff020203);
                layout.colours["gridTint"]           = juce::Colour (0xff6d726f);
                layout.params["gridBoost"]         = 7.0;
                layout.colours["curveColour"]        = mint;
                layout.colours["curveHighlight"]     = mint.brighter (0.5f);
                layout.colours["rollerIllumination"] = mint;
                layout.colours["modulationLamp"]     = mint;
                layout.colours["accent"]             = mint;
                layout.colours["labelInk"]           = bone;
                for (const char* id : { "typeLabel", "morphLabel", "qLabel", "gainLabel", "brandLabel" })
                    layout.elements[id].textColour = bone;
                layout.params["plateDark"]         = 1.0;
                layout.params["knobSvg"]           = 1.0;
                layout.params["zeroLine"]          = 1.0;
                layout.params["pixelTrace"]        = 1.0;
                layout.params["pixelCell"]         = 2.0;
            }
            if (juce::String (themeName).toLowerCase() == "rossum")
            {
                const juce::Colour blue (0xff2f72d8);
                const juce::Colour ink (0xff2b2d31);
                layout.colours["glassTop"]           = juce::Colour (0xff070708);
                layout.colours["glassBottom"]        = juce::Colour (0xff030304);
                layout.colours["gridTint"]           = juce::Colour (0xff3d63d6);
                layout.params["gridBoost"]         = 14.0;
                layout.colours["curveColour"]        = juce::Colours::white;
                layout.colours["curveHighlight"]     = juce::Colours::white;
                layout.colours["rollerIllumination"] = juce::Colour (0xff5a9cff);
                layout.colours["modulationLamp"]     = juce::Colour (0xff5a9cff);
                layout.colours["accent"]             = blue;
                layout.colours["labelInk"]           = ink;
                for (const char* id : { "typeLabel", "morphLabel", "qLabel", "gainLabel", "morphReadout", "qReadout", "typeName" })
                    layout.elements[id].textColour = ink;
                layout.elements["brandLabel"].textColour = blue;
                layout.params["plateSilver"]       = 1.0;
                layout.params["knobSilver"]        = 1.0;
                layout.params["knobDots"]          = 1.0;
                layout.params["graticule"]         = 1.0;
                layout.params["pixelTrace"]        = 1.0;
                layout.params["pixelCell"]         = 2.0;
            }
        }
        if (const char* plate = std::getenv ("TRENCH_PLATE"))
            if (juce::String (plate).toLowerCase() == "drawn")
                layout.params["plateDrawn"] = 1.0;
        if (const char* knob = std::getenv ("TRENCH_KNOB"))
        {
            const juce::String style = juce::String (knob).toLowerCase();
            if (style == "cream") { layout.params["knobSvg"] = 1.0; layout.params["knobStyle"] = 2.0; }
            if (style == "dark")  { layout.params["knobSvg"] = 1.0; layout.params["knobStyle"] = 1.0; }
        }
        if (const char* gainMode = std::getenv ("TRENCH_GAIN"))
            if (juce::String (gainMode).toLowerCase() != "pocket")
                layout.elements["gainLabel"].text = juce::String();
        if (const char* face = std::getenv ("TRENCH_FACE"))
            if (juce::String (face).toLowerCase() == "lean")
                layout.elements["gainLabel"].text = juce::String();
        if (const char* glowHex = std::getenv ("TRENCH_GLOW"))
        {
            const juce::String hex = juce::String (glowHex).trimCharactersAtStart ("#");
            if (hex.length() == 6)
            {
                const juce::Colour glow = juce::Colour ((juce::uint32) (0xff000000u | (juce::uint32) hex.getHexValue32()));
                const float hue = glow.getHue();
                const float sat = glow.getSaturation();
                layout.colours["accent"]             = glow;
                layout.colours["curveColour"]        = juce::Colour::fromHSV (hue, sat * 0.42f, 0.97f, 1.0f);
                layout.colours["curveHighlight"]     = juce::Colour::fromHSV (hue, sat * 0.15f, 1.0f, 1.0f);
                layout.colours["rollerIllumination"] = glow;
                layout.colours["modulationLamp"]     = glow;
                layout.colours["glassTop"]           = juce::Colour::fromHSV (hue, sat * 0.6f, 0.11f, 1.0f);
                layout.colours["glassBottom"]        = juce::Colour::fromHSV (hue, sat * 0.6f, 0.05f, 1.0f);
                layout.colours["gridTint"]           = juce::Colour::fromHSV (hue, sat * 0.7f, 0.80f, 1.0f);
            }
        }
        const auto envColour = [] (const char* name, juce::Colour& out)
        {
            const char* value = std::getenv (name);
            if (value == nullptr) return false;
            const juce::String hex = juce::String (value).trimCharactersAtStart ("#");
            if (hex.length() != 6) return false;
            out = juce::Colour ((juce::uint32) (0xff000000u | (juce::uint32) hex.getHexValue32()));
            return true;
        };
        juce::Colour glass, curve, grid, wheel;
        if (envColour ("TRENCH_GLASS", glass))
        {
            layout.colours["glassTop"]    = glass;
            layout.colours["glassBottom"] = glass.darker (0.55f);
            layout.colours["gridTint"]    = glass.brighter (1.6f).withSaturation (glass.getSaturation() * 0.8f);
        }
        if (envColour ("TRENCH_GRID", grid))
            layout.colours["gridTint"] = grid;
        if (const char* boost = std::getenv ("TRENCH_GRID_BOOST"))
            layout.params["gridBoost"] = juce::jlimit (1.0, 24.0, juce::String (boost).getDoubleValue());
        if (envColour ("TRENCH_CURVE", curve))
        {
            layout.colours["curveColour"]    = curve;
            layout.colours["curveHighlight"] = curve.brighter (0.5f);
        }
        if (envColour ("TRENCH_WHEEL", wheel))
        {
            layout.colours["rollerIllumination"] = wheel;
            layout.colours["modulationLamp"]     = wheel;
            layout.colours["accent"]             = wheel;
        }
        layout.strings["fontFamily"] = "Arial";
        layout.strings["fontFamilyEmphasis"] = "Arial";
        return layout;
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
};
}
