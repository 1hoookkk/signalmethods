#pragma once

#include "body_document.hpp"
#include "fit_controller.hpp"
#include "fit_room.hpp"
#include "response_plot.hpp"
#include "user_postures.hpp"
#include "trench/audio/audition.hpp"
#include "trench/core/measure.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/section_param.hpp"

#include <QList>
#include <QMainWindow>
#include <QUndoStack>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QAction;
class QAbstractButton;
class ChassisBar;
class MorphStrip;
class PostureList;
class SectionStrip;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(const std::filesystem::path& body_path,
                      double sample_rate_hz,
                      QWidget* parent = nullptr);

  [[nodiscard]] ResponsePlotWidget* responsePlot() const noexcept;
  [[nodiscard]] ChassisBar* chassisBar() const noexcept;
  [[nodiscard]] MorphStrip* morphStrip() const noexcept;
  [[nodiscard]] SectionStrip* sectionStrip(std::size_t section) const noexcept;
  [[nodiscard]] const std::filesystem::path& bodyPath() const noexcept;
  [[nodiscard]] BodyDocument* document() const noexcept;
  [[nodiscard]] FitController* fitController() const noexcept;
  [[nodiscard]] const trench::core::PackedBody& body() const noexcept;
  [[nodiscard]] QUndoStack* undoStack() noexcept;
  [[nodiscard]] std::uint32_t freedomMask() const noexcept;
  [[nodiscard]] bool fitRunning() const noexcept;

  void applySection(std::size_t section, const trench::core::PackedSection& words);
  void applyMask(std::size_t section, const trench::core::p2k::MaskParam& mask);
  void applyPoleHz(std::size_t section, double frequency_hz);
  void addResonance(double frequency_hz);
  void clearSection(std::size_t section);
  void selectSection(std::size_t section);
  void setCorner(std::size_t corner);
  bool saveBody(const std::filesystem::path& path);
  bool saveCorner(const std::filesystem::path& path);
  bool loadCorner(const std::filesystem::path& path);
  bool loadTarget(const std::filesystem::path& path);
  void setSourceModel(trench::core::measure::Source source);
  [[nodiscard]] trench::core::measure::Source sourceModel() const noexcept;
  void startFit();
  void openFitRoom();
  void applyVowel(const QString& symbol);
  void applyCharacter(double amount);
  [[nodiscard]] FitRoom* fitRoom() const noexcept;
  [[nodiscard]] PostureList* postureList() const noexcept;
  [[nodiscard]] int overlayCount() const noexcept;
  void selectOverlay(int index);
  void removeOverlay(int index);
  void stopAndKeep();
  void discardFit();
  void setAuditionGate(bool open);
  [[nodiscard]] bool auditionOpen() const noexcept;

 protected:
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;

 private:
  void chooseTarget();
  void addOverlay(const QString& name, std::vector<double> curve,
                  std::vector<double> marks_hz = {});
  void refreshFitRoom();
  void updateAudition();
  void saveBodyAs();
  void saveCornerAs();
  void chooseCorner();
  void updateVerbs();
  void updateProbes();
  void updateInterior();
  void updateStrips();
  void updatePostureMatch();
  void rebuildPostureGroups();
  void keepPosture();
  [[nodiscard]] std::vector<UserPostures::Pole> currentPolePosture() const;
  [[nodiscard]] std::optional<BodyDocument::CornerSnapshot> cornerWithPosture(
      const QString& symbol) const;
  [[nodiscard]] bool posturePolesHeld(const QString& symbol) const;
  void endRun();

  BodyDocument* document_{};
  FitController* fit_controller_{};
  std::filesystem::path body_path_;
  trench::core::PackedSection before_words_{};
  BodyDocument::CornerSnapshot pre_fit_{};
  std::size_t fit_corner_{};
  bool fit_active_{};
  trench::core::measure::Source source_model_{trench::core::measure::Source::kFlat};
  bool strip_gesture_{};
  bool character_gesture_{};
  std::array<BodyDocument::CornerSnapshot, 2> character_before_{};
  ResponsePlotWidget* response_plot_{};
  MorphStrip* morph_strip_{};
  PostureList* posture_list_{};
  QAbstractButton* keep_posture_{};
  UserPostures user_postures_;
  std::array<SectionStrip*, trench::core::kLegacySectionCount> strips_{};
  ChassisBar* chassis_bar_{};
  FitRoom* fit_room_{};
  std::unique_ptr<trench::audio::Audition> audition_;
  bool audition_open_{};
  std::optional<trench::audio::MonoClip> audition_clip_;
  QList<FitRoom::Overlay> overlays_;
  int selected_overlay_{-1};
  QAction* undo_action_{};
  QAction* redo_action_{};
};
