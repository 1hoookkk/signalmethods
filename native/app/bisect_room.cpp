#include "bisect_room.hpp"

#include "body_document.hpp"
#include "main_window.hpp"

#include <QApplication>
#include <QDateTime>
#include <QFont>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace {

const QColor kChassis{26, 31, 35};
const QColor kDot{118, 127, 131};

std::string axis_name(BisectRoom::Axis axis) {
  switch (axis) {
    case BisectRoom::Axis::kMorph: return "morph";
    case BisectRoom::Axis::kQ: return "q";
    case BisectRoom::Axis::kCharacter: return "character";
  }
  return "morph";
}

}  // namespace

std::optional<BisectRoom::Axis> BisectRoom::axisFromName(const QString& name) {
  if (name == QLatin1String("morph")) return Axis::kMorph;
  if (name == QLatin1String("q")) return Axis::kQ;
  if (name == QLatin1String("character")) return Axis::kCharacter;
  return std::nullopt;
}

BisectRoom::BisectRoom(MainWindow* window, Axis axis, std::filesystem::path body_path,
                       QWidget* parent)
    : QWidget(parent),
      window_(window),
      axis_(axis),
      body_path_(std::move(body_path)),
      session_(static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch())) {
  setWindowTitle(QStringLiteral("TRENCH"));
  setAutoFillBackground(true);
  QPalette chassis;
  chassis.setColor(QPalette::Window, kChassis);
  setPalette(chassis);
  setFocusPolicy(Qt::StrongFocus);
  resize(520, 360);

  pulse_timer_ = new QTimer(this);
  pulse_timer_->setInterval(33);
  connect(pulse_timer_, &QTimer::timeout, this, [this] {
    pulse_ += 0.09;
    update();
  });
  pulse_timer_->start();

  window_->setAuditionGate(true);
  beginTrial();
}

std::filesystem::path BisectRoom::curvePath() const {
  const auto stem = body_path_.stem().string();
  return std::filesystem::path(TRENCH_SOURCE_ROOT) / "dev" / "curves" /
         (axis_name(axis_) + "_" + stem + ".curve.json");
}

void BisectRoom::beginTrial() {
  candidate_ = session_.midpoint();
  driveTo(candidate_);
}

void BisectRoom::driveTo(double value) {
  auto* document = window_->document();
  switch (axis_) {
    case Axis::kMorph:
      document->setView(static_cast<float>(value), 0.0F);
      break;
    case Axis::kQ:
      document->setView(0.5F, static_cast<float>(value));
      break;
    case Axis::kCharacter:
      document->setView(0.5F, 1.0F);
      document->applyCharacter(value);
      break;
  }
}

void BisectRoom::nudge(double fraction) {
  const auto& trial = session_.current();
  const auto span = trial.hi - trial.lo;
  candidate_ = std::clamp(candidate_ + fraction * span, trial.lo, trial.hi);
  driveTo(candidate_);
}

void BisectRoom::finish() {
  const auto path = curvePath();
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  const auto text = trench::core::bisect::curve_json(
      axis_name(axis_), body_path_.stem().string(), session_.medians(), session_.repeats(),
      session_.monotone());
  std::ofstream out(path, std::ios::binary);
  out << text;
  out.close();
  window_->setAuditionGate(false);
  pulse_timer_->stop();
  done_ = true;
  update();
  QTimer::singleShot(700, qApp, &QCoreApplication::quit);
}

void BisectRoom::keyPressEvent(QKeyEvent* event) {
  if (done_) {
    event->accept();
    return;
  }
  const bool fine = (event->modifiers() & Qt::ShiftModifier) != 0;
  switch (event->key()) {
    case Qt::Key_A:
      driveTo(session_.current().lo);
      break;
    case Qt::Key_B:
      driveTo(session_.current().hi);
      break;
    case Qt::Key_C:
      driveTo(candidate_);
      break;
    case Qt::Key_Left:
      nudge(fine ? -0.004 : -0.02);
      break;
    case Qt::Key_Right:
      nudge(fine ? 0.004 : 0.02);
      break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      session_.accept(candidate_);
      if (session_.finished()) {
        finish();
      } else {
        beginTrial();
      }
      break;
    case Qt::Key_Escape:
      window_->setAuditionGate(false);
      QCoreApplication::quit();
      break;
    default:
      QWidget::keyPressEvent(event);
      return;
  }
  event->accept();
}

void BisectRoom::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), kChassis);
  if (done_) {
    QFont font = painter.font();
    font.setPointSizeF(13.0);
    font.setLetterSpacing(QFont::PercentageSpacing, 140.0);
    painter.setFont(font);
    painter.setPen(kDot);
    painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("done"));
    return;
  }
  const double breath = window_->auditionOpen() ? 0.5 + 0.5 * std::sin(pulse_) : 0.0;
  QColor ink = kDot;
  ink.setAlphaF(static_cast<float>(0.22 + 0.30 * breath));
  painter.setPen(Qt::NoPen);
  painter.setBrush(ink);
  const auto centre = rect().center();
  painter.drawEllipse(centre, 5, 5);
}
