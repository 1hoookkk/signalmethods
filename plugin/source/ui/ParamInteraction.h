#pragma once
#include "SelectorLookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
namespace trench::ui
{
inline bool isGainParameter (const juce::RangedAudioParameter* param)
{
    return param != nullptr && param->getLabel() == "dB";
}
inline float gainDragValue (juce::RangedAudioParameter& param, float& valueDb, float deltaY, bool fine)
{
    const auto& range = param.getNormalisableRange();
    valueDb = juce::jlimit (range.start, range.end, valueDb + deltaY * (fine ? 0.02f : 0.2f));
    const float next = ! fine && std::abs (valueDb) < 0.25f ? 0.0f : valueDb;
    return range.snapToLegalValue (next);
}
inline bool adjustParamFromKey (juce::RangedAudioParameter* param, const juce::KeyPress& key)
{
    if (param == nullptr) return false;
    const int code = key.getKeyCode();
    float next = param->getValue();
    const bool fine = key.getModifiers().isShiftDown();
    const float step = isGainParameter (param)
        ? (fine ? 0.1f : 0.5f) / (param->getNormalisableRange().end - param->getNormalisableRange().start)
        : (fine ? 0.001f : 0.01f);
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey) next -= step;
    else if (code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey) next += step;
    else if (code == juce::KeyPress::homeKey) next = 0.0f;
    else if (code == juce::KeyPress::endKey) next = 1.0f;
    else return false;
    param->beginChangeGesture();
    param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, next));
    param->endChangeGesture();
    return true;
}
inline void adjustParamFromWheel (juce::RangedAudioParameter* param, const juce::MouseEvent& e,
                                  const juce::MouseWheelDetails& wheel)
{
    if (param == nullptr || wheel.deltaY == 0.0f) return;
    float next = param->getValue() + wheel.deltaY * 0.05f;
    if (isGainParameter (param))
    {
        const bool fine = e.mods.isShiftDown();
        const float delta = wheel.isSmooth ? wheel.deltaY * (fine ? 1.0f : 5.0f)
                                          : std::copysign (fine ? 0.1f : 0.5f, wheel.deltaY);
        const auto& range = param->getNormalisableRange();
        const float db = range.snapToLegalValue (param->convertFrom0to1 (param->getValue()) + delta);
        next = param->convertTo0to1 (db);
    }
    param->beginChangeGesture();
    param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, next));
    param->endChangeGesture();
}
inline void resetParamToDefault (juce::RangedAudioParameter* p)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getDefaultValue());
    p->endChangeGesture();
}
inline void showParamContextMenu (juce::Component& owner, juce::RangedAudioParameter* param)
{
    if (param == nullptr) return;
    if (auto* ed = owner.findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* hc = ed->getHostContext())
            if (auto host = hc->getContextMenuForParameter (param))
            {
                std::shared_ptr<juce::HostProvidedContextMenu> keep = std::move (host);
                auto m = keep->getEquivalentPopupMenu();
                
                juce::SharedResourcePointer<SelectorLookAndFeel> look;
                juce::PopupMenu filtered;
                filtered.setLookAndFeel (&*look);
                juce::PopupMenu::MenuItemIterator it (m);
                while (it.next())
                {
                    auto& item = it.getItem();
                    if (item.text.containsIgnoreCase ("Value") || 
                        item.text.containsIgnoreCase ("Set..."))
                        continue;
                    if (item.isSeparator && filtered.getNumItems() > 0)
                    {
                        filtered.addItem (item);
                    }
                    else if (!item.isSeparator)
                    {
                        filtered.addItem (item);
                    }
                }

                if (filtered.getNumItems() > 0)
                    filtered.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&owner),
                                     [keep, look] (int) {});
            }
}
inline float fineDragScale (const juce::MouseEvent& e)
{
    return e.mods.isShiftDown() ? 0.2f : 1.0f;
}
}
