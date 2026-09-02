#include "skin.hpp"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace trench::app {
namespace {

void seat(QPalette& palette, QPalette::ColorRole role, const QColor& colour) {
  palette.setColor(QPalette::Active, role, colour);
  palette.setColor(QPalette::Inactive, role, colour);
  palette.setColor(QPalette::Disabled, role, colour);
}

}

void applyNinetiesSkin(QApplication& application) {
  application.setStyle(QStyleFactory::create(QStringLiteral("Windows")));

  QPalette palette;
  seat(palette, QPalette::Window, QColor(0xc0, 0xc0, 0xc0));
  seat(palette, QPalette::Button, QColor(0xc0, 0xc0, 0xc0));
  seat(palette, QPalette::Light, QColor(0xff, 0xff, 0xff));
  seat(palette, QPalette::Midlight, QColor(0xdf, 0xdf, 0xdf));
  seat(palette, QPalette::Mid, QColor(0xa0, 0xa0, 0xa0));
  seat(palette, QPalette::Dark, QColor(0x80, 0x80, 0x80));
  seat(palette, QPalette::Shadow, QColor(0x00, 0x00, 0x00));
  seat(palette, QPalette::Base, QColor(0xff, 0xff, 0xff));
  seat(palette, QPalette::Text, QColor(0x00, 0x00, 0x00));
  seat(palette, QPalette::WindowText, QColor(0x00, 0x00, 0x00));
  seat(palette, QPalette::ButtonText, QColor(0x00, 0x00, 0x00));
  seat(palette, QPalette::Highlight, QColor(0x00, 0x00, 0x80));
  seat(palette, QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x80, 0x80, 0x80));
  palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x80, 0x80, 0x80));
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x80, 0x80, 0x80));
  application.setPalette(palette);

  QFont font(QStringLiteral("Microsoft Sans Serif"), 8);
  font.setStyleStrategy(QFont::NoAntialias);
  application.setFont(font);
}

}
