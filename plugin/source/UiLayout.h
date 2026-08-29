#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
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

        layout.elements["morphWheel"]   = { { 109.8f, 675.1f, 442.8f, 99.7f }, {}, {} };
        layout.elements["qWheel"]       = { { 109.8f, 840.5f, 442.8f, 103.4f }, {}, {} };
        layout.elements["typeSelector"] = { { 230.0f, 139.0f, 666.6f, 68.0f },  {}, {} };

        layout.elements["colorRow"]     = { { 126.9f, 978.1f, 769.7f, 247.6f }, {}, {} };
        layout.elements["keyBox"]       = { { 551.6f, 65.0f, 345.0f, 62.0f }, {}, {} };

        layout.elements["morphReadout"] = { { 574.6f, 697.0f, 142.5f, 55.7f },  12.0f, juce::Colour (0xff2a2722) };
        layout.elements["qReadout"]     = { { 574.6f, 864.3f, 142.5f, 55.7f },  12.0f, juce::Colour (0xff2a2722) };

        layout.elements["spectrumGrid"] = { { 126.9f, 227.8f, 769.7f, 383.7f }, {}, {} };

        layout.elements["typeLabel"]    = { { 126.9f, 139.0f, 97.0f, 68.0f },  15.0f, juce::Colour (0xff2a2722) };
        layout.elements["typeLabel"].text = "BODY";

        layout.elements["typeName"]     = { { 244.0f, 143.0f, 522.0f, 64.0f },  18.0f, juce::Colour (0xff2a2722) };
        layout.elements["typeArrow"]    = { { 841.6f, 143.0f, 52.0f,  64.0f },  {}, {} };
        layout.elements["morphLabel"]   = { { 109.8f, 640.0f, 442.8f, 38.0f },  17.0f, juce::Colour (0xff2a2722) };
        layout.elements["morphLabel"].text = "MORPH";
        layout.elements["qLabel"]       = { { 109.8f, 798.0f, 442.8f, 38.0f },  17.0f, juce::Colour (0xff2a2722) };
        layout.elements["qLabel"].text = "Q";

        layout.elements["brandLabel"]   = { { 126.9f, 73.0f, 230.0f, 44.0f }, 18.5f, juce::Colour (0xff0f0c09) };

        layout.elements["brandLabel"].text = "TRENCH";
        layout.colours["accent"]             = juce::Colour (0xff3cc8be);
        layout.colours["curveColour"]        = juce::Colour (0xff3cc8be);
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
