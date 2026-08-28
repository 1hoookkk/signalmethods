#pragma once

#include <QColor>

#include <array>
#include <cstddef>

inline QColor section_color(std::size_t section) {
  static const std::array<QColor, 6> kStage{
      QColor{87, 222, 205},  QColor{247, 184, 92},  QColor{156, 130, 224},
      QColor{116, 200, 120}, QColor{232, 122, 148}, QColor{116, 174, 255}};
  return kStage[section % kStage.size()];
}
