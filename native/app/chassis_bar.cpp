#include "chassis_bar.hpp"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>

namespace {

const QColor kChassis{26, 31, 35};
const QColor kHairline{55, 63, 67};
const QColor kIdentity{174, 186, 190};
const QColor kIdentityDim{118, 127, 131};
const QColor kGhost{74, 82, 87};
const QColor kFit{87, 222, 205};
const QColor kKeep{232, 193, 74};
const QColor kDiscard{226, 78, 74};

constexpr double kBarHeight = 34.0;
constexpr double kPadHeight = 22.0;
constexpr double kPadPaddingX = 10.0;
constexpr double kPadGap = 8.0;
constexpr double kPadRadius = 2.0;

struct VerbLook {
  ChassisBar::Verb verb;
  const char* label;
  const QColor* ink;
};

const std::array<VerbLook, 4> kVerbs{{
    {ChassisBar::Verb::kTarget, "TARGET", &kIdentity},
    {ChassisBar::Verb::kFit, "FIT", &kFit},
    {ChassisBar::Verb::kKeep, "STOP & KEEP", &kKeep},
    {ChassisBar::Verb::kDiscard, "DISCARD", &kDiscard},
}};

QFont pad_font() { return QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold); }

}  // namespace

ChassisBar::ChassisBar(QWidget* parent) : QWidget(parent) {
  setFixedHeight(static_cast<int>(kBarHeight));
  setMouseTracking(true);
}

void ChassisBar::setBodyName(const QString& name) {
  body_name_ = name;
  update();
}

void ChassisBar::setTargetName(const QString& name) {
  target_name_ = name;
  update();
}

void ChassisBar::setState(bool has_target, bool running) {
  has_target_ = has_target;
  running_ = running;
  update();
}

std::vector<ChassisBar::Pad> ChassisBar::pads() const {
  const QFontMetricsF metrics(pad_font());
  std::vector<Pad> out;
  auto right = width() - 10.0;
  const auto top = (kBarHeight - kPadHeight) * 0.5;
  for (auto it = kVerbs.rbegin(); it != kVerbs.rend(); ++it) {
    const auto text_width = metrics.horizontalAdvance(QString::fromLatin1(it->label));
    const auto pad_width = text_width + 2.0 * kPadPaddingX;
    Pad pad;
    pad.verb = it->verb;
    pad.rect = QRectF(right - pad_width, top, pad_width, kPadHeight);
    switch (it->verb) {
      case Verb::kTarget:
        pad.available = !running_;
        break;
      case Verb::kFit:
        pad.available = has_target_ && !running_;
        break;
      case Verb::kKeep:
      case Verb::kDiscard:
        pad.available = running_;
        break;
    }
    out.push_back(pad);
    right -= pad_width + kPadGap;
  }
  return out;
}

std::optional<ChassisBar::Verb> ChassisBar::hit(const QPointF& at) const {
  for (const auto& pad : pads()) {
    if (pad.available && pad.rect.contains(at)) return pad.verb;
  }
  return std::nullopt;
}

void ChassisBar::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), kChassis);
  painter.setPen(QPen(kHairline, 1.0));
  painter.drawLine(QPointF{0.0, 0.5}, QPointF{static_cast<double>(width()), 0.5});

  const auto laid = pads();
  auto pads_left = width() - 10.0;
  for (const auto& pad : laid) pads_left = std::min(pads_left, pad.rect.left());

  painter.setFont(pad_font());
  const QFontMetricsF metrics(painter.font());
  auto x = 12.0;
  if (!body_name_.isEmpty()) {
    painter.setPen(kIdentity);
    const auto text = metrics.elidedText(body_name_, Qt::ElideMiddle, pads_left - x - 12.0);
    painter.drawText(QPointF{x, kBarHeight * 0.5 + metrics.ascent() * 0.5 - 1.0}, text);
    x += metrics.horizontalAdvance(text) + 14.0;
  }
  if (!target_name_.isEmpty() && x < pads_left - 40.0) {
    painter.setPen(QPen(kGhost, 1.2));
    painter.setBrush(Qt::NoBrush);
    const auto mark_y = kBarHeight * 0.5;
    painter.drawLine(QPointF{x, mark_y}, QPointF{x + 10.0, mark_y});
    x += 15.0;
    painter.setPen(kIdentityDim);
    const auto text = metrics.elidedText(target_name_, Qt::ElideMiddle, pads_left - x - 12.0);
    painter.drawText(QPointF{x, kBarHeight * 0.5 + metrics.ascent() * 0.5 - 1.0}, text);
  }

  for (const auto& pad : laid) {
    const auto* look = &kVerbs[0];
    for (const auto& candidate : kVerbs) {
      if (candidate.verb == pad.verb) look = &candidate;
    }
    auto ink = *look->ink;
    if (!pad.available) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(kGhost);
    } else {
      const auto hovered = hover_ == pad.verb;
      const auto is_pressed = pressed_ == pad.verb;
      if (is_pressed || hovered) {
        auto fill = ink;
        fill.setAlphaF(is_pressed ? 0.22 : 0.12);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(pad.rect, kPadRadius, kPadRadius);
      }
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(hovered || is_pressed ? ink : kHairline, 1.0));
      painter.drawRoundedRect(pad.rect.adjusted(0.5, 0.5, -0.5, -0.5), kPadRadius,
                              kPadRadius);
      painter.setPen(ink);
    }
    painter.drawText(pad.rect, Qt::AlignCenter, QString::fromLatin1(look->label));
  }
}

void ChassisBar::mouseMoveEvent(QMouseEvent* event) {
  const auto verb = hit(event->position());
  if (verb != hover_) {
    hover_ = verb;
    update();
  }
}

void ChassisBar::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  pressed_ = hit(event->position());
  if (pressed_) update();
}

void ChassisBar::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const auto released = hit(event->position());
  if (pressed_ && released == pressed_) emit verbClicked(*pressed_);
  pressed_.reset();
  update();
}

void ChassisBar::leaveEvent(QEvent*) {
  hover_.reset();
  pressed_.reset();
  update();
}
