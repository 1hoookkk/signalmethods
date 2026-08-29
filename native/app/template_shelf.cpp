#include "template_shelf.hpp"

#include "corpus_shelf.hpp"

#include "trench/core/formants.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace trench::app {

namespace {

QString qname(std::string_view name) {
  return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

TemplateEntry fromPosture(const trench::core::p2k::Posture& posture, QString name) {
  TemplateEntry entry;
  entry.name = std::move(name);
  for (std::size_t index = 0; index < entry.poles.size(); ++index) {
    if (index < posture.pole_count) {
      entry.poles[index] = {posture.poles[index].hz, posture.poles[index].bw_hz, true};
    } else {
      entry.poles[index] = {22050.0, 1000000000.0, false};
    }
  }
  return entry;
}

QString stripped(std::string_view name, QString prefix) {
  QString text = qname(name);
  if (text.startsWith(prefix)) text.remove(0, prefix.size());
  return text;
}

void appendCompiled(std::vector<TemplateEntry>& entries, const QString& name,
                    const QString& display) {
  for (std::size_t index = 0; index < kTemplateShelf.size(); ++index) {
    if (kTemplateShelf[index].name != name) continue;
    TemplateEntry entry = kTemplateShelf[index];
    if (!display.isEmpty()) entry.name = display;
    entries.push_back(std::move(entry));
    return;
  }
}

void appendByType(std::vector<TemplateEntry>& entries, std::string_view type,
                  const QString& prefix) {
  for (const auto& posture : trench::core::p2k::templates()) {
    if (posture.type != type) continue;
    entries.push_back(fromPosture(posture, stripped(posture.name, prefix)));
  }
}

}

std::vector<ShelfGroup> buildShelf() {
  std::vector<ShelfGroup> groups;

  ShelfGroup ipa{QString::fromUtf8("VOWELS \xc2\xb7 IPA"), {}};
  appendByType(ipa.entries, "VOWELS", QString());
  groups.push_back(std::move(ipa));

  static const std::array<QString, 12> kVowelKeys{
      {QStringLiteral("IY"), QStringLiteral("IH"), QStringLiteral("EI"), QStringLiteral("EH"),
       QStringLiteral("AE"), QStringLiteral("AH"), QStringLiteral("AW"), QStringLiteral("OA"),
       QStringLiteral("OO"), QStringLiteral("UW"), QStringLiteral("UH"), QStringLiteral("ER")}};

  ShelfGroup male{QString::fromUtf8("VOWELS \xc2\xb7 MALE"), {}};
  for (const QString& key : kVowelKeys) {
    appendCompiled(male.entries, QStringLiteral("MOUTH %1 M").arg(key), key);
  }
  groups.push_back(std::move(male));

  ShelfGroup female{QString::fromUtf8("VOWELS \xc2\xb7 FEMALE"), {}};
  for (const QString& key : kVowelKeys) {
    appendCompiled(female.entries, QStringLiteral("MOUTH %1 W").arg(key), key);
  }
  groups.push_back(std::move(female));

  ShelfGroup s1{QStringLiteral("MOUTHS S1"), {}};
  appendByType(s1.entries, "MOUTHS S1", QStringLiteral("s1 "));
  groups.push_back(std::move(s1));

  ShelfGroup s2{QStringLiteral("MOUTHS S2"), {}};
  appendByType(s2.entries, "MOUTHS S2", QStringLiteral("s2 "));
  groups.push_back(std::move(s2));

  ShelfGroup poses{QStringLiteral("POSES"), {}};
  appendByType(poses.entries, "POSES", QString());
  groups.push_back(std::move(poses));

  ShelfGroup bodies{QStringLiteral("BODIES"), {}};
  appendByType(bodies.entries, "BODIES", QString());
  groups.push_back(std::move(bodies));

  ShelfGroup objects{QStringLiteral("OBJECTS"), {}};
  appendByType(objects.entries, "OBJECTS", QString());
  groups.push_back(std::move(objects));

  ShelfGroup modal{QStringLiteral("MODAL"), {}};
  for (const QString& name :
       {QStringLiteral("MARIMBA"), QStringLiteral("VIBRAPHONE"), QStringLiteral("AGOGO"),
        QStringLiteral("WOOD ONE"), QStringLiteral("RESO"), QStringLiteral("WOOD TWO"),
        QStringLiteral("BEATS"), QStringLiteral("TWO FIXED"), QStringLiteral("CLUMP")}) {
    appendCompiled(modal.entries, name, QString());
  }
  groups.push_back(std::move(modal));

  ShelfGroup kit{QStringLiteral("KIT"), {}};
  for (const QString& name :
       {QStringLiteral("SPREAD RESONANT"), QStringLiteral("KLANG"), QStringLiteral("BASS CLOSED"),
        QStringLiteral("KLUB STACK"), QStringLiteral("BASS OPEN"), QStringLiteral("MOUTH OPEN"),
        QStringLiteral("MOUTH CLOSED"), QStringLiteral("KLATT MOUTH")}) {
    appendCompiled(kit.entries, name, QString());
  }
  groups.push_back(std::move(kit));

  ShelfGroup cubes{QStringLiteral("CUBES"), {}};
  appendByType(cubes.entries, "CUBES", QString());
  groups.push_back(std::move(cubes));

  ShelfGroup p2k{QStringLiteral("P2K"), {}};
  for (const auto& posture : trench::core::p2k::postures()) {
    QString name = qname(posture.name);
    const qsizetype space = name.indexOf(QLatin1Char(' '));
    if (space >= 0) name.remove(0, space + 1);
    p2k.entries.push_back(fromPosture(posture, std::move(name)));
  }
  for (const auto& posture : trench::core::p2k::compiled_vowels()) {
    p2k.entries.push_back(fromPosture(posture, stripped(posture.name, QStringLiteral("VOW "))));
  }
  groups.push_back(std::move(p2k));

  for (const char* group_name : {"REZ", "VOW", "EQ+", "EQ-", "LPF", "PHA", "FLG", "DST",
                                 "WAH", "SFX", "ARCHETYPES"}) {
    ShelfGroup corpus{QString::fromUtf8("P+Z \xc2\xb7 ") + QString::fromUtf8(group_name), {}};
    for (const auto& source : kCorpusShelf) {
      if (std::string_view(source.group) != std::string_view(group_name)) continue;
      TemplateEntry entry;
      entry.name = QString::fromUtf8(source.name);
      entry.poles = source.poles;
      entry.zeros = source.zeros;
      corpus.entries.push_back(std::move(entry));
    }
    groups.push_back(std::move(corpus));
  }

  return groups;
}

}
