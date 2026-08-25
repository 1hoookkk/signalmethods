#include "fit_controller.hpp"

#include <QMetaType>

#include <array>
#include <utility>
#include <vector>

namespace {

namespace p2k = trench::core::p2k;

QList<quint16> flatten(const p2k::CornerWords& words) {
  QList<quint16> out;
  out.reserve(static_cast<qsizetype>(p2k::kStageCount * words[0].size()));
  for (const auto& stage : words) {
    for (const auto word : stage) out.push_back(word);
  }
  return out;
}

}  // namespace

FitController::FitController(QObject* parent) : QObject(parent) {
  qRegisterMetaType<QList<quint16>>("QList<quint16>");
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

void FitController::startZeros(std::vector<double> ceiling, p2k::CornerWords roots,
                               std::uint32_t mask, p2k::Grid grid) {
  join();
  stop_.store(false);
  mask_.store(mask);
  const auto stamp = static_cast<quint64>(generation_.fetch_add(1) + 1);
  running_.store(true);

  worker_ = std::thread([this, stamp, ceiling = std::move(ceiling), roots,
                         grid = std::move(grid)]() mutable {
    bool ok = false;
    for (std::size_t pass = 0; pass < 8; ++pass) {
      std::uint32_t live_zeros = 0U;
      const auto freedom = mask_.load();
      for (std::size_t section = 0; section < p2k::kStageCount; ++section) {
        if ((freedom & p2k::zero_bit(section)) != 0U) live_zeros |= 1U << section;
      }
      const auto fit = p2k::fit_zeros_under(ceiling, roots, live_zeros, 1, grid);
      if (!fit) break;
      ok = true;
      if (fit->words == roots) break;
      roots = fit->words;
      emit previewReady(stamp, flatten(roots));
      if (stop_.load()) break;
    }
    running_.store(false);
    emit finished(stamp, ok, ok ? flatten(roots) : QList<quint16>{});
  });
}

void FitController::setMask(std::uint32_t mask) { mask_.store(mask); }

void FitController::requestStop() { stop_.store(true); }

void FitController::abandon() {
  generation_.fetch_add(1);
  stop_.store(true);
}
