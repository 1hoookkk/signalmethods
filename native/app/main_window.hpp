#pragma once

#include "response_plot.hpp"
#include "trench/core/packed_body.hpp"

#include <QMainWindow>
#include <QUndoStack>

#include <cstddef>
#include <cstdint>
#include <filesystem>

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(const std::filesystem::path& body_path,
                      double sample_rate_hz,
                      QWidget* parent = nullptr);

  [[nodiscard]] ResponsePlotWidget* responsePlot() const noexcept;
  [[nodiscard]] const trench::core::PackedBody& body() const noexcept;
  [[nodiscard]] QUndoStack* undoStack() noexcept;
  [[nodiscard]] std::uint32_t freedomMask() const noexcept;

  void applySection(std::size_t section, const trench::core::PackedSection& words);

 private:
  trench::core::PackedBody body_;
  double sample_rate_hz_{};
  std::uint32_t freedom_mask_{};
  trench::core::PackedSection before_words_{};
  ResponsePlotWidget* response_plot_{};
  QUndoStack undo_stack_;
};
