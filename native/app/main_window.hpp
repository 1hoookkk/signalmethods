#pragma once

#include "cascade_plot.hpp"
#include "editor_state.hpp"
#include "morph_pad.hpp"
#include "path_meter.hpp"
#include "row_table.hpp"
#include "trench/audio/audition.hpp"

#include <QMainWindow>

#include <filesystem>
#include <QString>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QPushButton;

class MainWindow final : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  void setAudition(bool enabled);
  void applyAuditionSource();
  void openDocument(const QString& chosen);
  void openPath(const QString& chosen);
  [[nodiscard]] EditorState& state() noexcept { return state_; }
  [[nodiscard]] CascadePlot* plot() const noexcept { return cascade_plot_; }
  [[nodiscard]] PathMeter* pathMeter() const noexcept { return path_meter_; }

 protected:
  void closeEvent(QCloseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  void openFile();
  void saveDocument();
  void resetDocument();
  void copyAcross();
  void exportBody240();
  void refresh();
  [[nodiscard]] trench::audio::AuditionView heardView() const;
  [[nodiscard]] trench::audio::AuditionView plotView() const;
  void updateAuditionView();
  void syncAuditionButton(bool checked);

  EditorState state_;
  CascadePlot* cascade_plot_{};
  MorphPad* morph_pad_{};
  PathMeter* path_meter_{};
  RowTable* row_table_{};
  QLabel* status_label_{};
  QPushButton* audition_button_{};
  QPushButton* solo_button_{};
  QString document_path_;
  std::unique_ptr<trench::audio::Audition> audition_;
};
