#pragma once
#include "BinaryData.h"
#include <juce_core/juce_core.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
namespace trench
{
struct BodyEntry
{
    const char* displayName;
    const char* base;
    const char* category;
    int behavior;
};
enum class TypeBehavior : int
{
    Static = 0,
    Dynamic,
    AutoQuarter,
    AutoHalf,
    Wobble,
    User,
};
enum class SecondaryTarget : int
{
    packed = 0,
    slam,
    packedAndSlam,
};
enum class MorphTaper : int
{
    linear = 0,
    log1p45,
};
struct BodyBehavior
{
    SecondaryTarget secondaryTarget = SecondaryTarget::packed;
    MorphTaper morphTaper = MorphTaper::linear;
};
inline constexpr const char* kAuditionBase = "@audition";
inline constexpr int kNoFilterIndex = 0;
inline constexpr int kDefaultBodyIndex = kNoFilterIndex;
inline constexpr const char* kNoFilterName = "No filter";
inline constexpr int kUserSlotPool = 128;
inline constexpr int kBodyParamMaxIndex = 511;
inline const BodyEntry* bakedRoster (int& countOut) noexcept;
inline juce::File userBodyDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("TRENCH").getChildFile ("User Bodies");
}
inline juce::File auditionSlotFile() noexcept
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("TRENCH")
               .getChildFile ("authoring_slot.json");
}
namespace detail
{
struct RosterStore
{
    std::vector<std::string> names;
    std::vector<std::string> bases;
    std::vector<std::string> categories;
    std::vector<BodyEntry> entries;
};
inline std::string prettyBodyName (const std::string& stem)
{
    juce::String s (stem);
    const int hashSeparator = s.lastIndexOf ("__");
    if (hashSeparator > 0)
    {
        const auto suffix = s.substring (hashSeparator + 2);
        if (suffix.length() == 12 && suffix.containsOnly ("0123456789abcdefABCDEF"))
            s = s.substring (0, hashSeparator);
    }
    const int us = s.indexOfChar ('_');
    if (us > 0 && s.substring (0, us) == s.substring (0, us).toUpperCase())
        s = s.substring (us + 1);
    s = s.replaceCharacter ('_', ' ');
    juce::String out;
    for (const auto& word : juce::StringArray::fromTokens (s, " ", {}))
    {
        if (out.isNotEmpty())
            out << ' ';
        out << (word == "to" ? word : word.substring (0, 1).toUpperCase() + word.substring (1));
    }
    return out.toStdString();
}
inline std::string bodyStem (const juce::File& file)
{
    auto name = file.getFileName();
    if (name.endsWithIgnoreCase (".cart.json"))
        return name.dropLastCharacters (10).toStdString();
    return file.getFileNameWithoutExtension().toStdString();
}
inline std::string bodyFolderCategory (const juce::File& root, const juce::File& file)
{
    auto parent = file.getParentDirectory().getRelativePathFrom (root)
                      .replaceCharacter ('\\', '/')
                      .trimCharactersAtStart ("/")
                      .trimCharactersAtEnd ("/");
    if (parent.isEmpty() || parent == ".")
        return std::string ("USER");
    return parent.toStdString();
}
inline bool isHiddenBodyFolder (const std::string& category)
{
    for (const auto& segment : juce::StringArray::fromTokens (juce::String (category), "/", {}))
        if (segment.startsWithChar ('.') || segment.startsWithChar ('_'))
            return true;
    return false;
}
inline void buildRosterStore (RosterStore& store)
{
    store.names.clear();
    store.bases.clear();
    store.categories.clear();
    store.entries.clear();
    int bakedCount = 0;
        const auto* baked = bakedRoster (bakedCount);
        for (int index = 0; index < bakedCount; ++index)
        {
            store.names.emplace_back (index == kNoFilterIndex
                                          ? std::string (baked[index].displayName)
                                          : prettyBodyName (baked[index].displayName));
            store.bases.emplace_back (baked[index].base);
            store.categories.emplace_back (baked[index].category);
        }
        store.entries.reserve (store.names.size());
        for (size_t i = 0; i < store.names.size(); ++i)
            store.entries.push_back ({ store.names[i].c_str(), store.bases[i].c_str(),
                                       store.categories[i].c_str(), (int) TypeBehavior::Static });
}
inline void appendUserBodies (RosterStore& store, const juce::File& directory)
{
    auto files = directory.findChildFiles (juce::File::findFiles, false, "*.body240");
    std::sort (files.begin(), files.end(), [] (const auto& a, const auto& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });
    for (const auto& file : files)
    {
        if (store.names.size() >= (size_t) kBodyParamMaxIndex + 1)
            break;
        if (file.getSize() != 240 || file.isHidden()
            || file.getFileName().startsWithChar ('_'))
            continue;
        const auto path = file.getFullPathName().toStdString();
        if (std::find (store.bases.begin(), store.bases.end(), path) != store.bases.end())
            continue;
        store.names.push_back (prettyBodyName (bodyStem (file)));
        store.bases.push_back (path);
        store.categories.emplace_back ("USER");
    }
    store.entries.clear();
    store.entries.reserve (store.names.size());
    for (size_t i = 0; i < store.names.size(); ++i)
        store.entries.push_back ({ store.names[i].c_str(), store.bases[i].c_str(),
                                  store.categories[i].c_str(), (int) TypeBehavior::Static });
}
struct RosterRegistry
{
    std::atomic<const RosterStore*> current { nullptr };
    std::mutex mutex;
    std::vector<std::unique_ptr<RosterStore>> versions;

    RosterRegistry()
    {
        auto store = std::make_unique<RosterStore>();
        buildRosterStore (*store);
        appendUserBodies (*store, userBodyDirectory());
        current.store (store.get(), std::memory_order_release);
        versions.push_back (std::move (store));
    }
    void refresh()
    {
        const std::lock_guard<std::mutex> lock (mutex);
        const auto* previous = current.load (std::memory_order_acquire);
        auto next = std::make_unique<RosterStore>(*previous);
        appendUserBodies (*next, userBodyDirectory());
        if (next->bases == previous->bases)
            return;
        const auto* published = next.get();
        versions.push_back (std::move (next));
        current.store (published, std::memory_order_release);
    }
};
inline RosterRegistry& rosterRegistry()
{
    static RosterRegistry registry;
    return registry;
}
inline const RosterStore& rosterStore()
{
    return *rosterRegistry().current.load (std::memory_order_acquire);
}
}
inline void rescanBodyRoster() { detail::rosterRegistry().refresh(); }
inline const BodyEntry* bakedRoster (int& countOut) noexcept
{
    static const BodyEntry entries[] = {
        { kNoFilterName, "identity", "SYSTEM", (int) TypeBehavior::Static },
#define TRENCH_PRESET(displayName, resourceStem, categoryName) \
        { displayName, resourceStem, categoryName, (int) TypeBehavior::Static },
#if TRENCH_DEV_PANEL
#include "../presets/PresetRosterDev.inc"
#else
#include "../presets/PresetRoster.inc"
#endif
#undef TRENCH_PRESET
    };
    countOut = (int) (sizeof (entries) / sizeof (entries[0]));
    return entries;
}
inline const BodyEntry* bodyRoster (int& countOut) noexcept
{
    auto& store = detail::rosterStore();
    countOut = (int) store.entries.size();
    return store.entries.data();
}
inline int bodyCount() noexcept
{
    int count = 0;
    bodyRoster (count);
    return count;
}
inline int wrapBodyIndex (int index) noexcept
{
    const auto count = bodyCount();
    if (count <= 0)
        return 0;
    index %= count;
    return index < 0 ? index + count : index;
}
inline bool bodyIsNoFilter (int index) noexcept { return wrapBodyIndex (index) == kNoFilterIndex; }
struct BakedReactSpec { int mode; float cutoffHz; };
inline BakedReactSpec bodyBakedReactSpec (int index)
{
    auto& store = detail::rosterStore();
    const auto& name = store.names[(size_t) wrapBodyIndex (index)];
    if (name == "De-Esser")  return { 2, 4000.0f };
    if (name == "De-Mudder") return { 3, 700.0f };
    return { 0, 0.0f };
}
inline bool bodyIsAudition (int) noexcept { return false; }
inline bool bodyRawBytes (int index, juce::MemoryBlock& out) noexcept
{
    int count = 0;
    const auto* roster = bodyRoster (count);
    if (count <= 0)
        return false;
    const auto& entry = roster[wrapBodyIndex (index)];
    if (juce::File::isAbsolutePath (entry.base))
    {
        const juce::File file (entry.base);
        if (file.existsAsFile() && file.hasFileExtension ("body240")
            && file.loadFileAsData (out) && out.getSize() == 240)
            return true;
        return false;
    }
    const auto wantedFilename = juce::String (entry.base) + ".body240";
    for (int resource = 0; resource < BinaryData::namedResourceListSize; ++resource)
    {
        if (wantedFilename != BinaryData::originalFilenames[resource])
            continue;
        int size = 0;
        const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[resource], size);
        if (data == nullptr || size != 240)
            return false;
        out.setSize (240);
        out.copyFrom (data, 0, 240);
        return true;
    }
    return false;
}
inline juce::String bodyCartridgeJson (int index) noexcept
{
    int count = 0;
    const auto* roster = bodyRoster (count);
    if (count <= 0)
        return {};
    const auto& entry = roster[wrapBodyIndex (index)];
    if (juce::File::isAbsolutePath (entry.base))
    {
        const juce::File file (entry.base);
        return file.hasFileExtension ("body240") ? juce::String() : file.loadFileAsString();
    }
    int size = 0;
    const auto* data = BinaryData::getNamedResource ((juce::String (entry.base) + "_json").toRawUTF8(), size);
    return data != nullptr && size > 0 ? juce::String::createStringFromData (data, size) : juce::String();
}
inline juce::String bodyDisplayName (int index) noexcept
{
    if (bodyIsNoFilter (index))
        return kNoFilterName;
    int count = 0;
    const auto* roster = bodyRoster (count);
    return count > 0 ? roster[wrapBodyIndex (index)].displayName : juce::String();
}
inline juce::String bodyBaseForIndex (int index) noexcept
{
    int count = 0;
    const auto* roster = bodyRoster (count);
    if (count <= 0 || index < 0 || index >= count)
        return {};
    return roster[index].base;
}
inline int bodyIndexForBase (const juce::String& base) noexcept
{
    if (base.isEmpty())
        return -1;
    int count = 0;
    const auto* roster = bodyRoster (count);
    for (int index = 0; index < count; ++index)
        if (base == roster[index].base)
            return index;
    return -1;
}
inline TypeBehavior bodyTypeBehavior (int index) noexcept
{
    if (bodyIsNoFilter (index))
        return TypeBehavior::Static;
    int count = 0;
    const auto* roster = bodyRoster (count);
    return count > 0 ? (TypeBehavior) roster[wrapBodyIndex (index)].behavior : TypeBehavior::Static;
}
inline bool bodyTypeHasMotion (int index) noexcept
{
    return bodyTypeBehavior (index) != TypeBehavior::Static;
}
inline bool bodyUsesLogMorph (int) noexcept { return false; }
inline bool bodySecondaryDrivesSlam (int) noexcept { return false; }
inline BodyBehavior fallbackBodyBehavior (int index) noexcept
{
    juce::ignoreUnused (index);
    return {};
}
inline SecondaryTarget secondaryTargetFromString (juce::String value) noexcept
{
    value = value.trim().toLowerCase();
    if (value == "slam" || value == "drive")
        return SecondaryTarget::slam;
    if (value == "packed+slam" || value == "packed_secondary_plus_slam" || value == "both")
        return SecondaryTarget::packedAndSlam;
    return SecondaryTarget::packed;
}
inline MorphTaper morphTaperFromString (juce::String value) noexcept
{
    value = value.trim().toLowerCase();
    return value == "log" || value == "log_1p45" ? MorphTaper::log1p45 : MorphTaper::linear;
}
inline BodyBehavior bodyBehaviorFromCartridgeJson (int index, const juce::String& json)
{
    auto behavior = fallbackBodyBehavior (index);
    const auto root = juce::JSON::parse (json);
    if (const auto* object = root.getDynamicObject())
    {
        const auto secondary = object->getProperty ("secondary_target");
        if (! secondary.isVoid())
            behavior.secondaryTarget = secondaryTargetFromString (secondary.toString());
        const auto taper = object->getProperty ("morph_taper");
        if (! taper.isVoid())
            behavior.morphTaper = morphTaperFromString (taper.toString());
    }
    return behavior;
}
inline bool secondaryTargetUsesPacked (SecondaryTarget target) noexcept
{
    return target == SecondaryTarget::packed || target == SecondaryTarget::packedAndSlam;
}
inline bool secondaryTargetUsesSlam (SecondaryTarget target) noexcept
{
    return target == SecondaryTarget::slam || target == SecondaryTarget::packedAndSlam;
}
inline float applyMorphTaper (MorphTaper taper, float morph) noexcept
{
    const auto value = juce::jlimit (0.0f, 1.0f, morph);
    return taper == MorphTaper::log1p45 ? std::pow (value, 1.45f) : value;
}
inline float bodyMorphForEngine (int index, float morph) noexcept
{
    return applyMorphTaper (fallbackBodyBehavior (index).morphTaper, morph);
}
inline bool bodyRawBytesFromCurrentPath (const juce::String& path, juce::MemoryBlock& out) noexcept
{
    const juce::File file (path);
    return file.existsAsFile() && file.hasFileExtension ("body240")
        && file.loadFileAsData (out) && out.getSize() == 240;
}
}
