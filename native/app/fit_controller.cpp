#include "fit_controller.hpp"

#include <QMetaType>

#include <array>
#include <optional>
#include <utility>

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

QList<quint16> flatten(const p2k::StoredCorner& words) {
  QList<quint16> out;
  out.reserve(static_cast<qsizetype>(p2k::kStageCount * p2k::kWordCount));
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

void FitController::start(std::vector<double> target, p2k::CornerWords seed,
                          std::uint32_t mask, p2k::Grid grid, p2k::RoleIntent intent) {
  join();
  stop_.store(false);
  mask_.store(mask);
  const auto stamp = static_cast<quint64>(generation_.fetch_add(1) + 1);
  running_.store(true);

  worker_ = std::thread([this, stamp, target = std::move(target), seed,
                         grid = std::move(grid), intent]() mutable {
    const std::array<p2k::Seed, 3> seeds{seed, p2k::SeedPeel{}, p2k::SeedContinuous{}};
    p2k::FitOptions options;
    options.allow_continuous = true;
    options.grid = &grid;
    options.intent = intent;

    const auto fit = p2k::fit_corner_watched(
        target, seeds, options, [this] { return mask_.load(); },
        [this] { return stop_.load(); },
        [this, stamp](const p2k::StepReport& report) {
          emit stepReady(stamp, static_cast<quint64>(report.section),
                         flatten(report.words));
        });

    running_.store(false);
    if (fit) {
      emit finished(stamp, true, flatten(p2k::packed_as_words(fit->packed)));
    } else {
      emit finished(stamp, false, QList<quint16>{});
    }
  });
}

void FitController::setMask(std::uint32_t mask) { mask_.store(mask); }

void FitController::requestStop() { stop_.store(true); }

void FitController::abandon() {
  generation_.fetch_add(1);
  stop_.store(true);
}
