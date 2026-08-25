#include "fit_controller.hpp"

#include <QMetaType>

#include <cmath>
#include <utility>

FitController::FitController(QObject* parent) : QObject(parent) {
  qRegisterMetaType<trench::core::native::Corner>("trench::core::native::Corner");
}

FitController::~FitController() {
  abandon();
  join();
}

bool FitController::running() const noexcept { return running_.load(); }

quint64 FitController::generation() const noexcept {
  return static_cast<quint64>(generation_.load());
}

void FitController::join() {
  if (worker_.joinable()) worker_.join();
}

void FitController::start(trench::core::FitTarget target,
                          trench::core::native::Corner corner,
                          std::uint32_t mask, double sample_rate_hz) {
  join();
  stop_.store(false);
  mask_.store(mask);
  const auto stamp = static_cast<quint64>(generation_.fetch_add(1) + 1);
  running_.store(true);

  worker_ = std::thread([this, stamp, target = std::move(target), corner,
                         sample_rate_hz]() mutable {
    const auto result = trench::core::native::fit_corner(
        target, corner, sample_rate_hz, [this] { return mask_.load(); },
        [this] { return stop_.load(); },
        [this, stamp](const trench::core::native::FitStep& step) {
          emit previewReady(stamp, step.corner,
                            static_cast<qsizetype>(step.section));
        });
    running_.store(false);
    emit finished(stamp, std::isfinite(result.final_loss), result.corner);
  });
}

void FitController::setMask(std::uint32_t mask) { mask_.store(mask); }

void FitController::requestStop() { stop_.store(true); }

void FitController::abandon() {
  generation_.fetch_add(1);
  stop_.store(true);
}
