#pragma once

#include "cascade_plot.hpp"
#include "armadillo_editor.hpp"
#include "editor_state.hpp"
#include "morph_pad.hpp"
#include "section_desk.hpp"
#include "section_strip.hpp"
#include "template_shelf.hpp"
#include "trench/audio/audition.hpp"

#include <QMainWindow>

#include <filesystem>
#include <QString>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QDoubleSpinBox;
class QLabel;
class QMenu;
class QPushButton;
class QToolButton;

class MainWindow final : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  bool loadReference(const std::filesystem::path& path);
  void setAudition(bool enabled);
  void applyAuditionSource();
  void chooseOverlay(std::optional<std::size_t> slot);
  void chooseTemplate(std::size_t slot);
  void openDocument(const QString& chosen);

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  struct KeptTemplate {
    QString name;
    std::vector<std::pair<double, double>> poles;
    std::vector<std::optional<std::pair<double, double>>> zeros;
  };

  struct Reference {
    QString name;
    std::vector<double> frequency_hz;
    std::vector<double> magnitude_db;
    std::optional<trench::audio::MonoClip> clip;
  };

  void openFile();
  void saveDocument();
  void resetDocument();
  void applyZeroHabits();
  void exportBody240();
  void analyzeReference();
  void adoptProposal();
  void clearProposal();
  void loadUserShelf();
  void keepTemplate();
  void refresh();
  void refreshInspector();
  void toggleSectionDesk();
  void updateAuditionView();
  void syncAuditionButton(bool checked);
  void setReference(Reference reference);
  void applyReferenceView();
  [[nodiscard]] trench::audio::MonoClip clipForDevice(
      const trench::audio::MonoClip& clip) const;

  EditorState state_;
  CascadePlot* cascade_plot_{};
  MorphPad* morph_pad_{};
  ArmadilloEditor* armadillo_editor_{};
  SectionStrip* section_strip_{};
  QLabel* reference_label_{};
  QLabel* status_label_{};
  QToolButton* template_shelf_{};
  QMenu* template_mine_{};
  QToolButton* overlay_shelf_{};
  QMenu* overlay_mine_{};
  QDoubleSpinBox* pole_frequency_{};
  QDoubleSpinBox* pole_bandwidth_{};
  QDoubleSpinBox* zero_frequency_{};
  QDoubleSpinBox* zero_bandwidth_{};
  QWidget* zero_frequency_group_{};
  QWidget* zero_bandwidth_group_{};
  QPushButton* tilt_button_{};
  QPushButton* analyze_button_{};
  QPushButton* sections_button_{};
  QPushButton* audition_button_{};
  QPushButton* clip_button_{};
  SectionDesk* section_desk_{};
  std::vector<std::pair<double, double>> proposal_poles_;
  std::vector<std::pair<double, double>> proposal_zeros_;
  std::vector<trench::app::TemplateEntry> shelf_;
  std::vector<KeptTemplate> user_shelf_;
  std::optional<Reference> reference_;
  QString document_path_;
  std::unique_ptr<trench::audio::Audition> audition_;
};
