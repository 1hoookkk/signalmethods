#pragma once

#include "cascade_plot.hpp"
#include "armadillo_editor.hpp"
#include "editor_state.hpp"
#include "section_strip.hpp"
#include "trench/audio/audition.hpp"

#include <QMainWindow>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

class MainWindow final : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  bool loadReference(const std::filesystem::path& path);
  void setAudition(bool enabled);

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  struct Reference {
    QString name;
    std::vector<double> frequency_hz;
    std::vector<double> magnitude_db;
    std::optional<trench::audio::MonoClip> clip;
  };

  void chooseReference();
  void refresh();
  void refreshInspector();
  void updateAuditionView();
  void setReference(Reference reference);
  [[nodiscard]] trench::audio::MonoClip clipForDevice(
      const trench::audio::MonoClip& clip) const;

  EditorState state_;
  CascadePlot* cascade_plot_{};
  ArmadilloEditor* armadillo_editor_{};
  SectionStrip* section_strip_{};
  QLabel* reference_label_{};
  QLabel* status_label_{};
  QPushButton* audition_button_{};
  QDoubleSpinBox* pole_frequency_{};
  QDoubleSpinBox* pole_bandwidth_{};
  QDoubleSpinBox* zero_frequency_{};
  QDoubleSpinBox* zero_bandwidth_{};
  std::optional<Reference> reference_;
  std::unique_ptr<trench::audio::Audition> audition_;
};
