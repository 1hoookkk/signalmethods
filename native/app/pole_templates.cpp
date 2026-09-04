#include "pole_templates.hpp"

#include "editor_state.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>
#include <variant>

#ifndef TRENCH_POLE_TEMPLATES
#define TRENCH_POLE_TEMPLATES ""
#endif
#ifndef TRENCH_FRAMES_X3
#define TRENCH_FRAMES_X3 ""
#endif
#ifndef TRENCH_KEYFRAMES_FROM_AUDIO
#define TRENCH_KEYFRAMES_FROM_AUDIO ""
#endif

namespace trench::app {

namespace {

constexpr double kEdgeDecayHz = 1.0;
constexpr double kCeilingZeroHz = 20'000.0;
constexpr double kResponseLowHz = 40.0;
constexpr double kResponseHighHz = 16'000.0;
constexpr std::size_t kResponsePoints = 96;

std::vector<double> sampleScratch(const EditorState& scratch, const std::vector<double>& hz) {
  const auto cascade = scratch.cascade(EditorState::kDatumHz);
  std::vector<double> out;
  out.reserve(hz.size());
  for (const double frequency : hz) {
    const double db = trench::core::cascade_response_db(
        std::span<const trench::core::Biquad>(cascade.data(), cascade.size()), frequency,
        EditorState::kDatumHz);
    out.push_back(std::isfinite(db) ? db : -120.0);
  }
  return out;
}

void birthZero(EditorState& state, std::size_t corner, std::size_t index, double hz,
               double bw_hz) {
  if (state.zeroPresentAt(corner, index)) return;
  state.selectSection(index);
  state.addZeroAt(hz, bw_hz);
  if (!state.zeroPresentAt(corner, index)) state.setZeroAt(corner, index, hz, bw_hz);
}

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

RowType rowTypeNamed(const QString& name) {
  if (name.compare(QStringLiteral("POLE"), Qt::CaseInsensitive) == 0) return RowType::kPole;
  if (name.compare(QStringLiteral("LP"), Qt::CaseInsensitive) == 0) return RowType::kLowPass;
  if (name.compare(QStringLiteral("HP"), Qt::CaseInsensitive) == 0) return RowType::kHighPass;
  return RowType::kEq;
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
  std::erase_if(out, [](const PoleTemplate& tpl) {
    return !tpl.ladder &&
           pickPoles(tpl, trench::core::native::kSections).size() < kTypeRowMinStates;
  });
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

std::vector<TypeRow> typeRowsFor(const PoleTemplate& tpl) {
  if (tpl.ladder || tpl.frame) return {};
  const auto states = pickPoles(tpl, trench::core::native::kSections);
  if (states.size() < kTypeRowMinStates) return {};
  std::vector<TypeRow> rows;
  rows.reserve(states.size());
  for (std::size_t i = 0; i + 1 < states.size(); ++i)
    rows.push_back({RowType::kEq, states[i].hz, states[i].bw_hz, kTypeRowSeedGainDb});
  rows.push_back({RowType::kLowPass, states.back().hz, states.back().bw_hz, 0.0});
  return rows;
}

void applyTypeRows(EditorState& state, const std::vector<TypeRow>& rows, std::size_t corner,
                   bool anchor) {
  constexpr std::size_t kSections = trench::core::native::kSections;
  if (corner >= trench::core::native::kCorners || rows.empty()) return;
  const std::size_t count = std::min(rows.size(), kSections);
  const bool ceiling = rows[count - 1].type == RowType::kLowPass;
  std::array<const TypeRow*, kSections> slot{};
  std::size_t next = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t target = (ceiling && i + 1 == count) ? kSections - 1 : next++;
    if (target >= kSections || slot[target] != nullptr) continue;
    slot[target] = &rows[i];
  }
  state.beginUndoGroup();
  for (std::size_t i = 0; i < kSections; ++i) {
    const bool want = slot[i] != nullptr;
    if (state.sectionEnabledAt(corner, i) != want) state.toggleSectionAt(corner, i);
  }
  for (std::size_t i = 0; i < kSections; ++i) {
    if (slot[i] == nullptr) continue;
    const TypeRow& row = *slot[i];
    const double bw_pole = std::max(1.0, row.bw_hz);
    state.setRootAt(corner, i, EditorState::Lane::kPole, row.hz, bw_pole);
    switch (row.type) {
      case RowType::kEq: {
        const double bw_zero = std::clamp(bw_pole * std::pow(10.0, row.gain_db / 20.0),
                                          EditorState::kMinBandwidthHz,
                                          EditorState::kMaxBandwidthHz);
        birthZero(state, corner, i, row.hz, bw_zero);
        state.setZeroAt(corner, i, row.hz, bw_zero);
        break;
      }
      case RowType::kLowPass:
        birthZero(state, corner, i, kCeilingZeroHz, bw_pole);
        state.setWordsAt(corner, i, EditorState::Lane::kZero,
                         trench::core::p2k::mag_word_for(kCeilingZeroHz,
                                                         trench::core::p2k::kS6ZeroRsqWord),
                         trench::core::p2k::kS6ZeroRsqWord);
        break;
      case RowType::kHighPass:
        birthZero(state, corner, i, row.hz, bw_pole);
        state.setRealRootAt(corner, i, EditorState::Lane::kZero, kEdgeDecayHz, kEdgeDecayHz);
        break;
      case RowType::kPole:
        if (state.zeroPresentAt(corner, i)) state.removeZeroAt(corner, i);
        break;
    }
  }
  if (anchor) state.anchorCornerToPartner(corner);
  state.endUndoGroup();
}

void applyPoleTemplate(EditorState& state, const PoleTemplate& tpl, std::size_t corner,
                       bool anchor) {
  if (!tpl.ladder) {
    const auto rows = typeRowsFor(tpl);
    if (rows.empty()) return;
    applyTypeRows(state, rows, corner, anchor);
    return;
  }
  const auto poles = ladderPoles(state, tpl, corner);
  if (poles.empty()) return;
  state.beginUndoGroup();
  for (std::size_t i = 0; i < trench::core::native::kSections; ++i) {
    const bool want = i < poles.size();
    if (state.sectionEnabledAt(corner, i) != want) state.toggleSectionAt(corner, i);
    if (want) {
      state.setRootAt(corner, i, EditorState::Lane::kPole, poles[i].hz,
                      std::max(1.0, poles[i].bw_hz));
      if (i < tpl.zero_bw_ratio.size()) {
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

EditorState::CornerState frameCornerState(const PoleTemplate& frame) {
  trench::core::PackedBody packed;
  for (auto& c : packed.words) c.fill(trench::core::kIdentitySection);
  for (std::size_t c = 0; c < trench::core::kCornerCount; ++c)
    for (std::size_t si = 0; si < 6; ++si)
      for (std::size_t wi = 0; wi < 5; ++wi)
        packed.words[c][si][wi] = frame.words[si][wi];
  const auto bytes = packed.legacy_bytes();
  const auto body = trench::core::native::import_p2k(
      std::span<const std::uint8_t>(bytes.data(), bytes.size()), 44'100.0);
  return EditorState::documentFrom(body).corners[0];
}

void applyFrame(EditorState& state, const PoleTemplate& frame, std::size_t corner,
                bool anchor) {
  if (!frame.frame || corner >= trench::core::native::kCorners) return;
  auto doc = state.document();
  doc.corners[corner] = frameCornerState(frame);
  state.beginUndoGroup();
  state.setDocument(doc);
  if (anchor) state.anchorCornerToPartner(corner);
  state.endUndoGroup();
}

std::vector<Keyframe> loadKeyframes() {
  std::vector<Keyframe> out;
  QFile file(QString::fromUtf8(TRENCH_KEYFRAMES_FROM_AUDIO));
  if (!file.open(QIODevice::ReadOnly)) return out;
  const auto doc = QJsonDocument::fromJson(file.readAll());
  for (const auto& item : doc.object().value(QStringLiteral("frames")).toArray()) {
    const auto o = item.toObject();
    Keyframe key;
    key.group = o.value(QStringLiteral("group")).toString();
    key.name = o.value(QStringLiteral("name")).toString();
    key.source = o.value(QStringLiteral("source")).toString();
    key.mode = o.value(QStringLiteral("mode")).toString();
    for (const auto& entry : o.value(QStringLiteral("rows")).toArray()) {
      const auto row = entry.toObject();
      key.rows.push_back({rowTypeNamed(row.value(QStringLiteral("type")).toString()),
                          row.value(QStringLiteral("hz")).toDouble(),
                          row.value(QStringLiteral("bw_hz")).toDouble(),
                          row.value(QStringLiteral("gain_db")).toDouble()});
    }
    if (!key.rows.empty()) out.push_back(std::move(key));
  }
  return out;
}

void applyKeyframe(EditorState& state, const Keyframe& key, std::size_t corner, bool anchor) {
  if (key.rows.empty() || corner >= trench::core::native::kCorners) return;
  applyTypeRows(state, key.rows, corner, anchor);
}

PoleTemplate keyframeFrame(const Keyframe& key) {
  PoleTemplate out;
  out.frame = true;
  out.family = key.group;
  out.type = key.name;
  out.datum_hz = EditorState::kDatumHz;
  for (auto& section : out.words) section = trench::core::kIdentitySection;
  if (key.rows.empty()) return out;
  EditorState scratch;
  scratch.setDocument(EditorState::blank());
  applyTypeRows(scratch, key.rows, 0, false);
  const auto& words = scratch.packed().words[0];
  for (std::size_t si = 0; si < 6; ++si) out.words[si] = words[si];
  return out;
}

std::array<std::uint16_t, 5> keyframeSlotWords(const Keyframe& key, std::size_t slot) {
  if (slot >= trench::core::native::kSections) return trench::core::kIdentitySection;
  return keyframeFrame(key).words[slot];
}

const std::vector<double>& responseGridHz() {
  static const std::vector<double> grid =
      trench::core::logarithmic_frequency_grid(kResponseLowHz, kResponseHighHz, kResponsePoints);
  return grid;
}

std::vector<double> responseDb(const PoleTemplate& tpl, const std::vector<double>& hz) {
  EditorState scratch;
  scratch.setDocument(EditorState::blank());
  if (tpl.frame) {
    applyFrame(scratch, tpl, 0, false);
  } else {
    applyPoleTemplate(scratch, tpl, 0, false);
  }
  return sampleScratch(scratch, hz);
}

std::vector<double> responseDb(const Keyframe& key, const std::vector<double>& hz) {
  EditorState scratch;
  scratch.setDocument(EditorState::blank());
  applyKeyframe(scratch, key, 0, false);
  return sampleScratch(scratch, hz);
}

}

