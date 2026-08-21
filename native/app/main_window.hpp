#pragma once

#include "body_document.hpp"
#include "fit_controller.hpp"
#include "response_plot.hpp"
#include "trench/core/packed_body.hpp"

#include <QList>
#include <QMainWindow>
#include <QUndoStack>

#include <cstddef>
#include <cstdint>
#include <filesystem>

class QAction;
class ChassisBar;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(const std::filesystem::path& body_path,
                      double sample_rate_hz,
                      QWidget* parent = nullptr);

  [[nodiscard]] ResponsePlotWidget* responsePlot() const noexcept;
  [[nodiscard]] BodyDocument* document() const noexcept;
  [[nodiscard]] FitController* fitController() const noexcept;
  [[nodiscard]] const trench::core::PackedBody& body() const noexcept;
  [[nodiscard]] QUndoStack* undoStack() noexcept;
  [[nodiscard]] std::uint32_t freedomMask() const noexcept;
  [[nodiscard]] bool fitRunning() const noexcept;

  void applySection(std::size_t section, const trench::core::PackedSection& words);
  bool loadTarget(const std::filesystem::path& path);
  void startFit();
  void stopAndKeep();
  void discardFit();

 private:
  void chooseTarget();
  void updateVerbs();
  void endRun();

  BodyDocument* document_{};
  FitController* fit_controller_{};
  trench::core::PackedSection before_words_{};
  BodyDocument::CornerSnapshot pre_fit_{};
  bool fit_active_{};
  ResponsePlotWidget* response_plot_{};
  ChassisBar* chassis_bar_{};
  QAction* undo_action_{};
  QAction* redo_action_{};
};
