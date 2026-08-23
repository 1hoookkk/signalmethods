#pragma once

#include <QColor>

#include <array>
#include <cstddef>

inline QColor section_color(std::size_t section) {
  static constexpr std::array<double, 6> kSectionHueDeg{8.0, 42.0, 88.0, 205.0, 300.0, 332.0};
  const auto index = section < kSectionHueDeg.size() ? section : std::size_t{0};
  return QColor::fromHsvF(kSectionHueDeg[index] / 360.0, 0.92, 1.0);
}
