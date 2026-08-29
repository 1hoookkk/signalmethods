#include "cascade_plot.hpp"

#include "trench/core/native_body.hpp"

#include <QFont>
#include <QFontMetrics>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QShortcut>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace {

constexpr double kLowHz = 20.0;
constexpr double kHighHz = 20'000.0;
constexpr double kNyquistHz = 22'050.0;

constexpr QColor kPanel{255, 255, 255};
constexpr QColor kPanelEdge{200, 200, 200};
constexpr QColor kGrid{230, 230, 230};
constexpr QColor kGridUnity{176, 176, 176};
constexpr QColor kText{64, 64, 64};
constexpr QColor kResponse{0, 0, 0};
constexpr QColor kAddressed{196, 103, 79};
constexpr QColor kReference{128, 128, 128};

constexpr double kLowDb = -30.0;
constexpr double kHighDb = 30.0;
constexpr int kStepDb = 10;

double finiteDb(double value) {
  if (!std::isfinite(value)) return value < 0.0 ? -400.0 : 400.0;
  return std::clamp(value, -400.0, 400.0);
}

const trench::core::native::Resonant* resonantOf(
    const trench::core::native::Roots& roots) {
  return std::get_if<trench::core::native::Resonant>(&roots);
}

}

CascadePlot::CascadePlot(EditorState* state, QWidget* parent)
    : QWidget(parent), state_(state) {
  setMinimumHeight(160);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  setFocusPolicy(Qt::ClickFocus);
  base_hz_ = trench::core::logarithmic_frequency_grid(kLowHz, kHighHz, 640);
  grid_hz_ = base_hz_;
  connect(state_, &EditorState::changed, this,
          qOverload<>(&CascadePlot::update));
  connect(state_, &EditorState::selectionChanged, this, [this] { update(); });
  for (const auto key : {Qt::Key_Delete, Qt::Key_Backspace}) {
    auto* drop_zero = new QShortcut(QKeySequence(key), this);
    drop_zero->setContext(Qt::WidgetShortcut);
    connect(drop_zero, &QShortcut::activated, state_, &EditorState::removeZero);
  }
}

void CascadePlot::setCascade(
    const trench::core::Cascade& cascade,
    const std::array<trench::core::Biquad,
                     trench::core::native::kSections>& sections,
    const std::array<bool, trench::core::native::kSections>& enabled,
    const std::vector<double>& seed_hz, std::size_t selected_section,
    double selected_frequency_hz, double sample_rate_hz) {
  sample_rate_hz_ = sample_rate_hz;
  enabled_ = enabled;
  selected_section_ = selected_section;
  selected_frequency_hz_ = selected_frequency_hz;
  grid_hz_ = base_hz_;
  grid_hz_.insert(grid_hz_.end(), seed_hz.begin(), seed_hz.end());
  std::sort(grid_hz_.begin(), grid_hz_.end());
  grid_hz_.erase(
      std::unique(grid_hz_.begin(), grid_hz_.end(),
                  [](double left, double right) {
                    return std::abs(right - left) < 0.01;
                  }),
      grid_hz_.end());
  response_db_.clear();
  response_db_.reserve(grid_hz_.size());
  const std::span<const trench::core::Biquad> six_sections{
      cascade.data(), trench::core::native::kSections};
  for (const double hz : grid_hz_) {
    response_db_.push_back(finiteDb(trench::core::cascade_response_db(
        six_sections, hz, sample_rate_hz)));
  }
  for (std::size_t section = 0; section < section_db_.size(); ++section) {
    section_db_[section].clear();
    if (!enabled_[section]) continue;
    section_db_[section].reserve(grid_hz_.size());
    const std::span<const trench::core::Biquad> one{&sections[section], 1};
    for (const double hz : grid_hz_) {
      section_db_[section].push_back(finiteDb(
          trench::core::cascade_response_db(one, hz, sample_rate_hz)));
    }
  }
  update();
}

void CascadePlot::setReference(std::vector<double> frequency_hz,
                               std::vector<double> magnitude_db) {
  reference_hz_ = std::move(frequency_hz);
  reference_db_ = std::move(magnitude_db);
  for (double& db : reference_db_) db = finiteDb(db);
  update();
}

void CascadePlot::setFormantMarks(std::vector<double> frequency_hz) {
  formant_hz_ = std::move(frequency_hz);
  update();
}

void CascadePlot::clearFormantMarks() {
  formant_hz_.clear();
  update();
}

void CascadePlot::clearReference() {
  reference_hz_.clear();
  reference_db_.clear();
  update();
}

QRectF CascadePlot::plotRect() const {
  return QRectF(rect()).adjusted(62.0, 14.0, -22.0, -38.0);
}

double CascadePlot::xForFrequency(double frequency_hz, const QRectF& plot) const {
  const double fraction = std::log(frequency_hz / kLowHz) /
                          std::log(kHighHz / kLowHz);
  return plot.left() + std::clamp(fraction, 0.0, 1.0) * plot.width();
}

double CascadePlot::yForDb(double db, const QRectF& plot, double low_db,
                           double high_db) const {
  const double fraction = (db - low_db) / (high_db - low_db);
  return plot.bottom() - fraction * plot.height();
}

double CascadePlot::frequencyForX(double x, const QRectF& plot) const {
  const double fraction =
      std::clamp((x - plot.left()) / plot.width(), 0.0, 1.0);
  return kLowHz * std::pow(kHighHz / kLowHz, fraction);
}

double CascadePlot::dbForY(double y, const QRectF& plot, double low_db,
                           double high_db) const {
  const double fraction =
      std::clamp((plot.bottom() - y) / plot.height(), 0.0, 1.0);
  return low_db + fraction * (high_db - low_db);
}

double CascadePlot::responseDbAt(double frequency_hz) const {
  if (grid_hz_.empty() || response_db_.size() != grid_hz_.size()) return 0.0;
  std::size_t nearest = 0;
  double best = std::numeric_limits<double>::max();
  for (std::size_t index = 0; index < grid_hz_.size(); ++index) {
    const double distance = std::abs(grid_hz_[index] - frequency_hz);
    if (distance < best) {
      best = distance;
      nearest = index;
    }
  }
  return response_db_[nearest];
}

std::vector<CascadePlot::Handle> CascadePlot::zeroHandles() const {
  std::vector<Handle> result;
  if (state_ == nullptr) return result;
  const QRectF plot = plotRect();
  for (std::size_t section = 0; section < trench::core::native::kSections;
       ++section) {
    if (!state_->sectionEnabled(section)) continue;
    if (!state_->rootPresent(section, EditorState::Lane::kZero)) continue;
    const auto* zero = resonantOf(state_->section(section).zero);
    if (zero == nullptr) continue;
    const double x = xForFrequency(std::clamp(zero->hz, kLowHz, kHighHz), plot);
    const double y = std::clamp(
        yForDb(responseDbAt(zero->hz), plot, kLowDb, kHighDb), plot.top(),
        plot.bottom());
    result.push_back(Handle{section, QPointF{x, y}});
  }
  return result;
}

std::optional<CascadePlot::Handle> CascadePlot::hitHandle(
    const QPointF& position) const {
  std::optional<Handle> closest;
  double closest_distance = 14.0;
  for (const Handle& handle : zeroHandles()) {
    const double distance = QLineF{position, handle.position}.length();
    if (distance <= closest_distance) {
      closest = handle;
      closest_distance = distance;
    }
  }
  return closest;
}

double CascadePlot::solveBandwidth(std::size_t section, double frequency_hz,
                                   double target_db) const {
  const auto& authored = state_->section(section);
  if (section + 1 == trench::core::native::kSections) {
    const auto* zero = resonantOf(authored.zero);
    return zero == nullptr ? EditorState::kMinBandwidthHz : zero->bw_hz;
  }
  std::vector<trench::core::Biquad> others;
  for (std::size_t index = 0; index < trench::core::native::kSections;
       ++index) {
    if (index == section || !state_->sectionEnabled(index)) continue;
    others.push_back(state_->sectionBiquad(index));
  }
  const double base = finiteDb(trench::core::cascade_response_db(
      std::span<const trench::core::Biquad>{others.data(), others.size()},
      frequency_hz, sample_rate_hz_));
  const auto error = [&](double bandwidth_hz) {
    trench::core::native::Section candidate = authored;
    candidate.zero = trench::core::native::Resonant{frequency_hz, bandwidth_hz};
    const trench::core::Biquad own = trench::core::native::biquad(
        trench::core::native::design(candidate, sample_rate_hz_));
    const std::span<const trench::core::Biquad> one{&own, 1};
    const double total =
        base + finiteDb(trench::core::cascade_response_db(one, frequency_hz,
                                                          sample_rate_hz_));
    return std::abs(total - target_db);
  };

  constexpr int kProbes = 48;
  const double span =
      EditorState::kMaxBandwidthHz / EditorState::kMinBandwidthHz;
  int best = 0;
  double best_error = std::numeric_limits<double>::max();
  for (int probe = 0; probe < kProbes; ++probe) {
    const double bandwidth_hz =
        EditorState::kMinBandwidthHz *
        std::pow(span, static_cast<double>(probe) / (kProbes - 1));
    const double value = error(bandwidth_hz);
    if (value < best_error) {
      best_error = value;
      best = probe;
    }
  }
  const auto probe_hz = [&](int probe) {
    return EditorState::kMinBandwidthHz *
           std::pow(span, static_cast<double>(std::clamp(probe, 0, kProbes - 1)) /
                              (kProbes - 1));
  };
  double low = probe_hz(best - 1);
  double high = probe_hz(best + 1);
  for (int step = 0; step < 12; ++step) {
    const double ratio = high / low;
    const double first = low * std::pow(ratio, 1.0 / 3.0);
    const double second = low * std::pow(ratio, 2.0 / 3.0);
    if (error(first) <= error(second)) {
      high = second;
    } else {
      low = first;
    }
  }
  return std::clamp(std::sqrt(low * high), EditorState::kMinBandwidthHz,
                    EditorState::kMaxBandwidthHz);
}

void CascadePlot::applyPointer(const QPointF& position) {
  if (!drag_) return;
  const QRectF plot = plotRect();
  const double frequency_hz = frequencyForX(position.x(), plot);
  const double target_db = dbForY(position.y(), plot, kLowDb, kHighDb);
  state_->setRoot(drag_->section, EditorState::Lane::kZero, frequency_hz,
                  solveBandwidth(drag_->section, frequency_hz, target_db));
}

void CascadePlot::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const auto hit = hitHandle(event->position());
  if (!hit) {
    drag_.reset();
    return;
  }
  state_->selectRoot(hit->section, EditorState::Lane::kZero);
  drag_ = hit;
  state_->beginUndoGroup();
  grabMouse();
}

void CascadePlot::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || hitHandle(event->position())) return;
  const std::size_t section = state_->selectedSection();
  if (!state_->sectionEnabled(section)) return;
  if (state_->rootPresent(section, EditorState::Lane::kZero)) return;
  const QRectF plot = plotRect();
  const double frequency_hz = frequencyForX(event->position().x(), plot);
  const double target_db = dbForY(event->position().y(), plot, kLowDb, kHighDb);
  state_->beginUndoGroup();
  state_->addZeroAt(frequency_hz, 100.0);
  state_->setRoot(section, EditorState::Lane::kZero, frequency_hz,
                  solveBandwidth(section, frequency_hz, target_db));
  state_->endUndoGroup();
}

void CascadePlot::mouseMoveEvent(QMouseEvent* event) {
  if (drag_) {
    applyPointer(event->position());
    return;
  }
  const auto hit = hitHandle(event->position());
  setCursor(hit ? Qt::OpenHandCursor : Qt::ArrowCursor);
  if (!hit) return;
  const auto* zero = resonantOf(state_->section(hit->section).zero);
  if (zero == nullptr) return;
  QToolTip::showText(event->globalPosition().toPoint(),
                     QStringLiteral("S%1 ZERO  ·  %2 Hz  ·  %3 Hz BW")
                         .arg(hit->section + 1)
                         .arg(zero->hz, 0, 'f', 2)
                         .arg(zero->bw_hz, 0, 'f', 2),
                     this);
}

void CascadePlot::mouseReleaseEvent(QMouseEvent* event) {
  if (!drag_ || event->button() != Qt::LeftButton) return;
  applyPointer(event->position());
  state_->endUndoGroup();
  drag_.reset();
  releaseMouse();
  setCursor(Qt::ArrowCursor);
}

void CascadePlot::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), palette().window().color());
  const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setPen(Qt::NoPen);
  painter.setBrush(kPanel);
  painter.drawRect(card);
  painter.setPen(QPen(kPanelEdge, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(card);

  const QRectF plot = plotRect();

  const double low_db = kLowDb;
  const double high_db = kHighDb;

  painter.setPen(QPen(kGrid, 1.0));

  constexpr std::array<double, 11> frequency_lines{
      20.0, 40.0, 80.0, 160.0, 320.0, 640.0, 1'280.0,
      2'560.0, 5'120.0, 10'240.0, 20'480.0};
  for (const double hz : frequency_lines) {
    const double x = xForFrequency(hz, plot);
    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
    QString label = hz >= 1000.0
                        ? QStringLiteral("%1k").arg(hz / 1000.0, 0, 'g', 2)
                        : QString::number(static_cast<int>(hz));
    if (hz == kNyquistHz) label = QStringLiteral("NYQ");
    painter.setPen(kText);
    painter.drawText(QRectF{x - 28.0, plot.bottom() + 8.0, 56.0, 18.0},
                     Qt::AlignHCenter | Qt::AlignTop, label);
    painter.setPen(QPen(kGrid, 1.0));
  }
  const double step = static_cast<double>(kStepDb);
  const int first_db = static_cast<int>(std::ceil(low_db / step) * step);
  for (int db = first_db; db <= static_cast<int>(high_db); db += kStepDb) {
    const double y = yForDb(static_cast<double>(db), plot, low_db, high_db);
    const bool unity = db == 0;
    painter.setPen(QPen(unity ? kGridUnity : kGrid, 1.0));
    painter.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
    painter.setPen(kText);
    painter.drawText(QRectF{8.0, y - 9.0, 46.0, 18.0},
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("%1 dB").arg(db));
    painter.setPen(QPen(kGrid, 1.0));
  }

  const auto draw_curve = [&](const std::vector<double>& hz,
                              const std::vector<double>& db,
                              const QPen& pen) {
    if (hz.size() != db.size() || hz.size() < 2) return;
    QPainterPath path;
    bool started = false;
    for (std::size_t index = 0; index < hz.size(); ++index) {
      if (!(hz[index] >= kLowHz && hz[index] <= kHighHz) ||
          !std::isfinite(db[index])) {
        continue;
      }
      const QPointF point{xForFrequency(hz[index], plot),
                          yForDb(db[index], plot, low_db, high_db)};
      if (!started) {
        path.moveTo(point);
        started = true;
      } else {
        path.lineTo(point);
      }
    }
    painter.save();
    painter.setClipRect(plot);
    painter.setPen(pen);
    painter.drawPath(path);
    painter.restore();
  };

  painter.setBrush(Qt::NoBrush);

  for (const double hz : formant_hz_) {
    if (hz < kLowHz || hz > kHighHz) continue;
    const double x = xForFrequency(hz, plot);
    painter.setPen(QPen(kReference, 1.0));
    painter.drawLine(QPointF{x, plot.top()}, QPointF{x, plot.top() + 9.0});
    painter.setBrush(kReference);
    painter.drawEllipse(QPointF{x, plot.top() + 11.5}, 2.2, 2.2);
    painter.setBrush(Qt::NoBrush);
  }

  QPen reference_pen(kReference, 1.0, Qt::DashLine);
  reference_pen.setCosmetic(true);
  reference_pen.setCapStyle(Qt::FlatCap);
  draw_curve(reference_hz_, reference_db_, reference_pen);
  if (enabled_[selected_section_]) {
    QPen section_pen(kAddressed, 1.0);
    section_pen.setCosmetic(true);
    section_pen.setCapStyle(Qt::FlatCap);
    draw_curve(grid_hz_, section_db_[selected_section_], section_pen);
  }
  QPen response_pen(kResponse, 1.0);
  response_pen.setCosmetic(true);
  response_pen.setCapStyle(Qt::FlatCap);
  draw_curve(grid_hz_, response_db_, response_pen);

  const bool zero_selected =
      state_->selectedLane() == EditorState::Lane::kZero;
  for (const Handle& handle : zeroHandles()) {
    const bool addressed =
        handle.section == state_->selectedSection() && zero_selected;
    const double radius = addressed ? 6.5 : 4.5;
    painter.setPen(addressed ? QPen(Qt::NoPen) : QPen(kResponse, 1.0));
    painter.setBrush(addressed ? QBrush(kAddressed) : QBrush(Qt::NoBrush));
    QPainterPath diamond;
    diamond.moveTo(handle.position + QPointF{0.0, -radius});
    diamond.lineTo(handle.position + QPointF{radius, 0.0});
    diamond.lineTo(handle.position + QPointF{0.0, radius});
    diamond.lineTo(handle.position + QPointF{-radius, 0.0});
    diamond.closeSubpath();
    painter.drawPath(diamond);
  }
}
