#include "pole_templates.hpp"

#include "editor_state.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/packed_body.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <span>
#include <variant>

#ifndef TRENCH_POLE_TEMPLATES
#define TRENCH_POLE_TEMPLATES ""
#endif
#ifndef TRENCH_FRAMES_X3
#define TRENCH_FRAMES_X3 ""
#endif

namespace trench::app {

namespace {

std::vector<PoleState> readStates(const QJsonArray& array) {
  std::vector<PoleState> out;
  for (const auto& item : array) {
    const auto o = item.toObject();
    out.push_back({o.value(QStringLiteral("hz")).toDouble(),
                   o.value(QStringLiteral("bw_hz")).toDouble(),
                   o.value(QStringLiteral("r")).toDouble(),
                   o.value(QStringLiteral("bodies")).toInt()});
  }
  return out;
}

void readFamily(const QJsonObject& root, const QString& key, const QString& family, double datum,
                std::vector<PoleTemplate>& out) {
  const auto group = root.value(key).toObject();
  for (auto it = group.begin(); it != group.end(); ++it) {
    const auto o = it.value().toObject();
    PoleTemplate tpl;
    tpl.family = family;
    tpl.type = it.key();
    tpl.datum_hz = datum;
    for (const auto& b : o.value(QStringLiteral("bodies")).toArray()) tpl.bodies.push_back(b.toString());
    tpl.states = readStates(o.value(QStringLiteral("states")).toArray());
    if (!tpl.states.empty()) out.push_back(std::move(tpl));
  }
}

}

std::vector<PoleTemplate> loadPoleTemplates() {
  std::vector<PoleTemplate> out;
  QFile file(QString::fromUtf8(TRENCH_POLE_TEMPLATES));
  if (!file.open(QIODevice::ReadOnly)) return out;
  const auto doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) return out;
  const auto root = doc.object();
  readFamily(root, QStringLiteral("p2k_44100"), QStringLiteral("X3"), 44'100.0, out);
  readFamily(root, QStringLiteral("morpheus_39062_5"), QStringLiteral("MORPHEUS"), 39'062.5, out);
  const auto ladders = root.value(QStringLiteral("ladders")).toObject();
  for (auto it = ladders.begin(); it != ladders.end(); ++it) {
    const auto o = it.value().toObject();
    PoleTemplate tpl;
    tpl.family = QStringLiteral("LADDER");
    tpl.type = it.key();
    tpl.ladder = true;
    tpl.root_hz = o.value(QStringLiteral("default_root_hz")).toDouble(64.3);
    for (const auto& r : o.value(QStringLiteral("ratios")).toArray()) tpl.ratios.push_back(r.toDouble());
    for (const auto& b : o.value(QStringLiteral("bw_fraction")).toArray()) tpl.bw_fraction.push_back(b.toDouble());
    for (const auto& z : o.value(QStringLiteral("zero_bw_ratio")).toArray()) tpl.zero_bw_ratio.push_back(z.toDouble());
    for (std::size_t i = 0; i < tpl.ratios.size(); ++i)
      tpl.states.push_back({tpl.root_hz * tpl.ratios[i], tpl.root_hz * tpl.ratios[i] * (i < tpl.bw_fraction.size() ? tpl.bw_fraction[i] : 0.03), 0.0, 1});
    if (!tpl.ratios.empty()) out.push_back(std::move(tpl));
  }
  return out;
}

std::vector<PoleState> pickPoles(const PoleTemplate& tpl, std::size_t count) {
  std::vector<PoleState> chosen = tpl.states;
  std::sort(chosen.begin(), chosen.end(),
            [](const PoleState& a, const PoleState& b) { return a.bodies > b.bodies; });
  std::vector<PoleState> out;
  for (const auto& s : chosen) {
    const bool close = std::any_of(out.begin(), out.end(), [&](const PoleState& o) {
      return std::abs(std::log2(o.hz / s.hz)) < 0.25;
    });
    if (close) continue;
    out.push_back(s);
    if (out.size() == count) break;
  }
  std::sort(out.begin(), out.end(), [](const PoleState& a, const PoleState& b) { return a.hz < b.hz; });
  return out;
}

std::vector<PoleState> ladderPoles(EditorState& state, const PoleTemplate& tpl, std::size_t corner) {
  double root = tpl.root_hz;
  if (state.sectionEnabledAt(corner, 0)) {
    if (const auto* res = std::get_if<trench::core::native::Resonant>(&state.sectionAt(corner, 0).pole);
        res != nullptr && res->hz >= EditorState::kLowHz) {
      root = res->hz;
    }
  }
  std::vector<PoleState> out;
  for (std::size_t i = 0; i < tpl.ratios.size() && out.size() < trench::core::native::kSections; ++i) {
    const double hz = root * tpl.ratios[i];
    if (hz > 0.45 * EditorState::kDatumHz) break;
    const double frac = i < tpl.bw_fraction.size() ? tpl.bw_fraction[i] : 0.03;
    out.push_back({hz, std::max(1.0, hz * frac), 0.0, 1});
  }
  return out;
}

void applyPoleTemplate(EditorState& state, const PoleTemplate& tpl, std::size_t corner) {
  const auto poles = tpl.ladder ? ladderPoles(state, tpl, corner) : pickPoles(tpl, trench::core::native::kSections);
  if (poles.empty()) return;
  state.beginUndoGroup();
  for (std::size_t i = 0; i < trench::core::native::kSections; ++i) {
    const bool want = i < poles.size();
    if (state.sectionEnabledAt(corner, i) != want) state.toggleSectionAt(corner, i);
    if (want) {
      state.setRootAt(corner, i, EditorState::Lane::kPole, poles[i].hz,
                      std::max(1.0, poles[i].bw_hz));
      if (tpl.ladder && i < tpl.zero_bw_ratio.size()) {
        const double ratio = tpl.zero_bw_ratio[i];
        if (ratio > 0.0) {
          const double zbw = std::max(1.0, poles[i].bw_hz * ratio);
          if (!state.zeroPresentAt(corner, i)) {
            state.selectSection(i);
            state.addZeroAt(poles[i].hz, zbw);
          }
          state.setZeroAt(corner, i, poles[i].hz, zbw);
        } else if (state.zeroPresentAt(corner, i)) {
          state.removeZeroAt(corner, i);
        }
      }
    }
  }
  state.endUndoGroup();
}


std::vector<PoleTemplate> loadFrames() {
  std::vector<PoleTemplate> out;
  QFile file(QString::fromUtf8(TRENCH_FRAMES_X3));
  if (!file.open(QIODevice::ReadOnly)) return out;
  const auto doc = QJsonDocument::fromJson(file.readAll());
  for (const auto& item : doc.object().value(QStringLiteral("frames")).toArray()) {
    const auto o = item.toObject();
    PoleTemplate f;
    f.frame = true;
    f.family = o.value(QStringLiteral("type")).toString();
    f.type = o.value(QStringLiteral("body")).toString() + QStringLiteral(" ") + o.value(QStringLiteral("corner")).toString();
    f.datum_hz = 44'100.0;
    const auto sections = o.value(QStringLiteral("sections")).toArray();
    for (int si = 0; si < 6 && si < sections.size(); ++si) {
      const auto words = sections[si].toArray();
      for (int wi = 0; wi < 5 && wi < words.size(); ++wi)
        f.words[(std::size_t) si][(std::size_t) wi] = static_cast<std::uint16_t>(words[wi].toInt());
    }
    out.push_back(std::move(f));
  }
  return out;
}

void applyFrame(EditorState& state, const PoleTemplate& frame, std::size_t corner) {
  if (!frame.frame || corner >= trench::core::native::kCorners) return;
  trench::core::PackedBody packed;
  for (auto& c : packed.words) c.fill(trench::core::kIdentitySection);
  for (std::size_t c = 0; c < trench::core::kCornerCount; ++c)
    for (std::size_t si = 0; si < 6; ++si)
      for (std::size_t wi = 0; wi < 5; ++wi)
        packed.words[c][si][wi] = frame.words[si][wi];
  const auto bytes = packed.legacy_bytes();
  const auto body = trench::core::native::import_p2k(std::span<const std::uint8_t>(bytes.data(), bytes.size()), 44'100.0);
  const auto seeded = EditorState::documentFrom(body);
  auto doc = state.document();
  doc.corners[corner] = seeded.corners[0];
  state.setDocument(doc);
}

}

