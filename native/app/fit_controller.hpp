#pragma once

#include "trench/core/fit_target.hpp"
#include "trench/core/native_fit.hpp"

#include <QMetaType>
#include <QObject>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

class FitController final : public QObject {
  Q_OBJECT

 public:
  explicit FitController(QObject* parent = nullptr);
  ~FitController() override;

  [[nodiscard]] bool running() const noexcept;
  [[nodiscard]] quint64 generation() const noexcept;

  void start(trench::core::FitTarget target, trench::core::native::Corner corner,
             std::uint32_t mask, double sample_rate_hz);
  void setMask(std::uint32_t mask);
  void requestStop();
  void abandon();

 signals:
  void previewReady(quint64 generation, trench::core::native::Corner corner,
                    qsizetype section);
  void finished(quint64 generation, bool ok, trench::core::native::Corner corner);

 private:
  void join();

  std::thread worker_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> running_{false};
  std::atomic<std::uint32_t> mask_{trench::core::native::kAllFree};
  std::atomic<std::uint64_t> generation_{0};
};

Q_DECLARE_METATYPE(trench::core::native::Corner)
