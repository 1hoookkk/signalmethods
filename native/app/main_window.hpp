#pragma once

#include "body_document.hpp"
#include "fit_controller.hpp"
#include "response_plot.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/packed_body.hpp"

#include <QList>
#include <QMainWindow>
#include <QUndoStack>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

class QAction;
class QTableView;
class ChassisBar;
class MorphStrip;
class SectionModel;
class SpaceDock;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(const std::filesystem::path& body_path,
                      double sample_rate_hz,
                      QWidget* parent = nullptr);

  [[nodiscard]] ResponsePlotWidget* responsePlot() const noexcept;
  [[nodiscard]] ChassisBar* chassisBar() const noexcept;
  [[nodiscard]] MorphStrip* morphStrip() const noexcept;
  [[nodiscard]] SpaceDock* spaceDock() const noexcept;
  [[nodiscard]] SectionModel* sectionModel() const noexcept;
  [[nodiscard]] const std::filesystem::path& bodyPath() const noexcept;
  [[nodiscard]] BodyDocument* document() const noexcept;
  [[nodiscard]] FitController* fitController() const noexcept;
  [[nodiscard]] const trench::core::PackedBody& body() const noexcept;
  [[nodiscard]] QUndoStack* undoStack() noexcept;
  [[nodiscard]] std::uint32_t freedomMask() const noexcept;
  [[nodiscard]] bool fitRunning() const noexcept;

  [[nodiscard]] double dcDriftDb() const noexcept;

  void applySection(std::size_t section, const trench::core::PackedSection& words);
  void setCorner(std::size_t corner);
  bool saveBody(const std::filesystem::path& path);
  bool loadTarget(const std::filesystem::path& path);
  void setSourceModel(trench::core::measure::Source source);
  [[nodiscard]] trench::core::measure::Source sourceModel() const noexcept;
  void startFit();
  void stopAndKeep();
  void discardFit();
  void renormalizeDc();

 private:
  void chooseTarget();
  void saveBodyAs();
  void updateVerbs();
  void updateProbes();
  void updateInterior();
  void endRun();

  BodyDocument* document_{};
  FitController* fit_controller_{};
  std::filesystem::path body_path_;
  trench::core::PackedSection before_words_{};
  BodyDocument::CornerSnapshot pre_fit_{};
  std::size_t fit_corner_{};
  bool fit_active_{};
  double dc_drift_db_{};
  trench::core::measure::Source source_model_{trench::core::measure::Source::kFlat};
  ResponsePlotWidget* response_plot_{};
  MorphStrip* morph_strip_{};
  SectionModel* section_model_{};
  QTableView* section_table_{};
  SpaceDock* space_dock_{};
  ChassisBar* chassis_bar_{};
  QAction* undo_action_{};
  QAction* redo_action_{};
};
