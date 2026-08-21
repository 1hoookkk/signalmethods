#pragma once

#include "trench/core/p2k.hpp"

#include <QList>
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

  void start(std::vector<double> target, trench::core::p2k::CornerWords seed,
             std::uint32_t mask);
  void setMask(std::uint32_t mask);
  void requestStop();
  void abandon();

 signals:
  void stepReady(quint64 generation, quint64 section, QList<quint16> words);
  void finished(quint64 generation, bool ok, QList<quint16> words);

 private:
  void join();

  std::thread worker_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> running_{false};
  std::atomic<std::uint32_t> mask_{trench::core::p2k::kAllFree};
  std::atomic<std::uint64_t> generation_{0};
};
