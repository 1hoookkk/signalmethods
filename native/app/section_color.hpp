#pragma once

#include <QColor>

#include <cstddef>

inline QColor section_color(std::size_t section) {
  return QColor::fromHsvF(static_cast<double>(section) / 7.0, 0.92, 1.0);
}
