#pragma once

#include "cascade_plot.hpp"
#include "editor_state.hpp"
#include "keyframe_grid.hpp"
#include "morph_pad.hpp"
#include "path_meter.hpp"
#include "rows_table.hpp"
#include "trench/audio/audition.hpp"

#include <QMainWindow>

#include <filesystem>
#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QDragEnterEvent;
class QDropEvent;
class QAction;
class QLabel;
class QPushButton;
class QCheckBox;
class QTimer;
class QToolButton;

class MainWindow final : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  void setAudition(bool enabled);
  void applyAuditionSource();
  void openDocument(const QString& chosen);
  void openPath(const QString& chosen);
  void seedFromAudio(const std::vector<float>& mono, double sample_rate_hz);
  void setFromAudioMode(int mode);
  [[nodiscard]] int fromAudioMode() const noexcept;
  void setAxisNamesForTest(const QString& morph, const QString& q);
  void openFrames();
  [[nodiscard]] trench::audio::AuditionView frameView(const KeyframeGrid::Entry& entry) const;
  [[nodiscard]] trench::audio::AuditionView frameView(const trench::app::Keyframe& key) const;
  [[nodiscard]] bool auditionRunning() const noexcept;
  std::function<void(const trench::audio::AuditionView&)> auditionTap;
  [[nodiscard]] KeyframeGrid* keyframeGrid() const noexcept { return keyframe_grid_; }
  [[nodiscard]] EditorState& state() noexcept { return state_; }
  [[nodiscard]] CascadePlot* plot() const noexcept { return cascade_plot_; }
  [[nodiscard]] PathMeter* pathMeter() const noexcept { return path_meter_; }
  [[nodiscard]] MorphPad* pad() const noexcept { return morph_pad_; }

 protected:
  void closeEvent(QCloseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  void openFile();
  void closeFrames();
  void seedFromAudioFile(const QString& path);
  void saveDocument();
  void resetDocument();
  void copyAcross();
  void exportBody240();
  void refresh();
  void landOnEditingCorner(const std::function<void(std::size_t, bool)>& land);
  void landOnCorner(std::size_t corner, const std::function<void(std::size_t, bool)>& land);
  void landPick(const KeyframeGrid::Entry& entry, bool partner);
  void landFrame(const trench::app::Keyframe& key, bool partner);
  void setPlotReference(std::vector<double> db_on_grid);
  void restorePlotReference();
  [[nodiscard]] bool anchoring() const;
  [[nodiscard]] trench::audio::AuditionView heardView() const;
  [[nodiscard]] trench::audio::AuditionView plotView() const;
  void updateAuditionView();
  void pushAuditionView(trench::audio::AuditionView view);
  void syncAuditionButton(bool checked);

  EditorState state_;
  CascadePlot* cascade_plot_{};
  MorphPad* morph_pad_{};
  PathMeter* path_meter_{};
  RowsTable* rows_table_{};
  QLabel* status_label_{};
  QToolButton* from_audio_button_{};
  QToolButton* anchor_button_{};
  QAction* six_bells_action_{};
  QAction* speech_action_{};
  QPushButton* audition_button_{};
  QPushButton* solo_button_{};
  QPushButton* frames_button_{};
  KeyframeGrid* keyframe_grid_{};
  std::vector<double> plot_reference_;
  bool grid_started_audition_{};
  QString audition_line_;
  QCheckBox* slot_box_{};
  QTimer* slot_timer_{};
  void pushSlot();
  QString document_path_;
  std::unique_ptr<trench::audio::Audition> audition_;
};
