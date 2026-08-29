#include "body_io.hpp"

#include "trench/core/packed_body.hpp"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>
#include <variant>

namespace trench::app {
namespace {

using Resonant = trench::core::native::Resonant;
using SectionWords = std::array<std::uint16_t, trench::core::kCoefficientCount>;
using CornerWords = std::array<SectionWords, trench::core::native::kSections>;

constexpr double kRateHz = trench::core::kP2kDatumHz;
constexpr double kSectionScale = 0.53;
constexpr double kLowHz = 20.0;
constexpr double kHighHz = 21'000.0;
constexpr double kAnchorFloor = 0.85;
constexpr double kAnchorCeiling = 1.15;
constexpr Resonant kParkPole{12'000.0, 3'000.0};
constexpr double kParkZeroHz = 9'642.0;
constexpr double kParkZeroRadius = 0.81;

Resonant parkZero() {
  return {kParkZeroHz, -std::log(kParkZeroRadius) * kRateHz / std::numbers::pi};
}

std::pair<std::uint16_t, std::uint16_t> encodeRoot(const Resonant& root) {
  const double radius = std::exp(-std::numbers::pi * root.bw_hz / kRateHz);
  const double angle =
      2.0 * std::numbers::pi * std::clamp(root.hz, kLowHz, kHighHz) / kRateHz;
  const double q = radius * radius;
  const double p = -2.0 * radius * std::cos(angle);
  const double dr = 1.0 - q;
  const double dm = (p + 2.0 - dr) / 4.0;
  return {trench::core::encode_word(dm), trench::core::encode_word(dr)};
}

double quantisedLaneAtDc(std::uint16_t magnitude_word,
                         std::uint16_t radius_word) {
  const double dm = trench::core::decode_word(magnitude_word);
  const double dr = trench::core::decode_word(radius_word);
  const double q = 1.0 - dr;
  const double p = 4.0 * dm + dr - 2.0;
  return 1.0 + p + q;
}

double cornerDcGain(const CornerWords& words) {
  double gain = 1.0;
  for (const SectionWords& section : words) {
    gain *= 4.0 * trench::core::decode_word(section[4]) *
            quantisedLaneAtDc(section[0], section[1]) /
            quantisedLaneAtDc(section[2], section[3]);
  }
  return gain;
}

QString refuseWrite(const QString& path) {
  return QStringLiteral("WRITE FAILED · %1")
      .arg(QFileInfo(path).fileName().toUpper());
}

}  // namespace

QString saveBody240(const EditorState& state, const QString& path) {
  std::array<CornerWords, trench::core::native::kCorners> body{};
  for (std::size_t corner = 0; corner < trench::core::native::kCorners;
       ++corner) {
    for (std::size_t index = 0; index < trench::core::native::kSections;
         ++index) {
      const bool live = state.sectionEnabledAt(corner, index);
      const auto& section = state.sectionAt(corner, index);
      const auto pole =
          encodeRoot(live ? std::get<Resonant>(section.pole) : kParkPole);
      const auto zero =
          encodeRoot(live && state.zeroPresentAt(corner, index)
                         ? std::get<Resonant>(section.zero)
                         : parkZero());
      body[corner][index] = {zero.first, zero.second, pole.first, pole.second,
                             trench::core::encode_word(kSectionScale / 4.0)};
    }
    const double seated = cornerDcGain(body[corner]);
    if (!std::isfinite(seated) || seated == 0.0) {
      return QStringLiteral("DC ANCHOR FAILED · CORNER %1").arg(corner);
    }
    const double factor = std::pow(
        std::abs(seated), 1.0 / double(trench::core::native::kSections));
    const std::uint16_t scale_word =
        trench::core::encode_word((kSectionScale / factor) / 4.0);
    for (SectionWords& section : body[corner]) section[4] = scale_word;
    const double anchored = std::abs(cornerDcGain(body[corner]));
    if (!std::isfinite(anchored) || anchored < kAnchorFloor ||
        anchored > kAnchorCeiling) {
      return QStringLiteral("DC ANCHOR FAILED · CORNER %1").arg(corner);
    }
  }

  std::array<std::uint8_t, trench::core::kLegacyBodyBytes> bytes{};
  std::size_t offset = 0;
  for (const CornerWords& corner : body) {
    for (const SectionWords& section : corner) {
      for (const std::uint16_t word : section) {
        bytes[offset++] = static_cast<std::uint8_t>(word & 0xFFU);
        bytes[offset++] = static_cast<std::uint8_t>(word >> 8U);
      }
    }
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return refuseWrite(path);
  }
  const qint64 written = file.write(
      reinterpret_cast<const char*>(bytes.data()),
      static_cast<qint64>(bytes.size()));
  file.close();
  if (written != static_cast<qint64>(bytes.size())) return refuseWrite(path);
  return {};
}

}  // namespace trench::app
