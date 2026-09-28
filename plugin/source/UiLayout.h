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

        layout.elements["morphWheel"]   = { { 117.4f, 676.6f, 426.2f, 74.0f }, {}, {} };
        layout.elements["qWheel"]       = { { 117.4f, 840.6f, 426.2f, 76.0f }, {}, {} };
        layout.elements["morphWell"]    = { { 115.0f, 673.0f, 430.0f, 80.0f }, {}, {} };
        layout.elements["qWell"]        = { { 115.0f, 837.0f, 430.0f, 82.0f }, {}, {} };
        layout.elements["typeSelector"] = { { 233.2f, 137.1f, 675.8f, 67.1f },  {}, {} };

        layout.elements["keyBox"]       = { { 559.2f, 64.1f, 349.8f, 61.2f }, {}, {} };

        layout.elements["morphReadout"] = { { 563.2f, 684.6f, 174.4f, 61.2f },  15.5f, juce::Colour (0xff4a3520) };
        layout.elements["qReadout"]     = { { 563.2f, 849.6f, 174.4f, 61.2f },  15.5f, juce::Colour (0xff4a3520) };

        layout.elements["spectrumGrid"] = { { 128.1f, 245.6f, 768.7f, 342.7f }, {}, {} };

        layout.elements["typeLabel"]    = { { 128.7f, 137.1f, 98.3f, 67.1f },  12.9f, juce::Colour (0xff2a2722) };
        layout.elements["typeLabel"].text = "BODY";

        layout.elements["typeName"]     = { { 247.4f, 141.1f, 529.2f, 63.1f },  20.0f, juce::Colour (0xff1a1713) };
        layout.elements["typeArrow"]    = { { 853.3f, 141.1f, 52.7f,  63.1f },  {}, {} };
        layout.elements["morphLabel"]   = { { 111.3f, 621.25f, 448.9f, 48.0f },  14.6f, juce::Colour (0xff2a2722) };
        layout.elements["morphLabel"].text = "MORPH";
        layout.elements["qLabel"]       = { { 111.3f, 786.25f, 448.9f, 48.0f },  14.6f, juce::Colour (0xff2a2722) };
        layout.elements["qLabel"].text = "Q";
        layout.elements["inputLabel"]   = { { 159.3f, 1120.0f, 260.0f, 48.0f },  13.9f, juce::Colour (0xff2a2722) };
        layout.elements["inputLabel"].text = "INPUT (dB)";
        layout.elements["outputLabel"]  = { { 433.3f, 1120.0f, 260.0f, 48.0f },  13.9f, juce::Colour (0xff2a2722) };
        layout.elements["outputLabel"].text = "OUTPUT (dB)";
        layout.elements["inputKnob"]    = { { 199.3f, 1165.0f, 180.0f, 180.0f }, {}, {} };
        layout.elements["outputKnob"]   = { { 473.3f, 1165.0f, 180.0f, 180.0f }, {}, {} };
        layout.elements["inputReadout"] = { { 202.3f, 1307.0f, 174.4f, 61.2f },  15.5f, juce::Colour (0xff4a3520) };
        layout.elements["outputReadout"] = { { 476.3f, 1307.0f, 174.4f, 61.2f },  15.5f, juce::Colour (0xff4a3520) };
        layout.elements["slamButton"]   = { { 384.3f, 1232.0f, 84.0f, 46.0f }, {}, {} };

        layout.elements["brandLabel"]   = { { 128.7f, 65.1f, 304.2f, 55.2f }, 17.5f, juce::Colour (0xff0f0c09) };

        layout.elements["brandLabel"].text = "TRENCH";
        juce::Colour lamp (0xff44dede);
        juce::Colour trace (0xe6b5ffff);
        if (const char* accent = std::getenv ("TRENCH_ACCENT"))
        {
            const juce::String hex = juce::String (accent).trimCharactersAtStart ("#");
            if (hex.length() == 6)
            {
                lamp = juce::Colour ((juce::uint32) (0xff000000u | (juce::uint32) hex.getHexValue32()));
                trace = lamp;
            }
        }
        layout.colours["accent"]             = lamp;
        layout.colours["curveColour"]        = trace;
        layout.colours["curveHighlight"]     = trace;
        layout.colours["telemetry"]          = juce::Colour (0xff5d8080);
        layout.colours["dashed"]             = juce::Colour (0xff5d8080);
        layout.colours["rollerIllumination"] = lamp;
        layout.colours["modulationLamp"]     = lamp;
        layout.colours["phosphor"]           = juce::Colour (0xffa8cbcb);

        layout.colours["amber"]              = juce::Colour (0xffb8862e);
        layout.colours["screenEdge"]         = juce::Colour (0xff171325);

        layout.colours["spectrumGhost"]      = juce::Colour (0xff8fd8d8);
        layout.colours["glassTop"]           = juce::Colour (0xff318786);
        layout.colours["glassBottom"]        = juce::Colour (0xff1d504f);
        layout.colours["labelInk"]    = juce::Colour (0xff1e262c);

        layout.colours["wellTop"]            = juce::Colour (0xffcbd8e6);
        layout.colours["wellBottom"]         = juce::Colour (0xff9db0c4);
        layout.colours["wellKeyline"]        = juce::Colour (0xffe6eef6);
        layout.params["wellRadius"]        = 9.0;
        layout.params["readoutAliasScale"] = 0.95;
        layout.params["typeArrowExtra"]    = 6.0;
        layout.params["curveDbTop"]        = 75.0;
        layout.params["curveDbBottom"]     = -75.0;
        layout.params["fontBold"]          = 1.0;
        layout.params["gridBoost"]         = 10.0;
        layout.colours["gridTint"]           = juce::Colour (0xff276467);
#if TRENCH_DEV_PANEL
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
                for (const char* id : { "typeLabel", "morphLabel", "qLabel", "brandLabel" })
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
                for (const char* id : { "typeLabel", "morphLabel", "qLabel", "morphReadout", "qReadout", "typeName" })
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
#endif
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
