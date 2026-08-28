#pragma once

#include "trench/core/fit_target.hpp"
#include "trench/core/native_body.hpp"

#include <QList>
#include <QString>
#include <QWidget>

#include <optional>
#include <vector>

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;

class FitRoom final : public QWidget {
  Q_OBJECT

 public:
  enum class SourceKind { kCurve, kAudioEnvelope, kTransfer, kBody };

  struct Overlay {
    QString name;
    trench::core::FitTarget target;
    std::vector<double> marks_hz;
    std::vector<trench::core::native::Resonant> suggested_poles;
    SourceKind source{SourceKind::kCurve};
    std::optional<std::size_t> corner;
    std::optional<trench::core::native::Body> body;
    QString provenance;
    bool reveal_response{true};
    bool reveal_poles{false};
    bool reveal_zeros{false};
  };

  explicit FitRoom(QWidget* parent = nullptr);

  void setOverlays(QList<Overlay> overlays, int selected);

  [[nodiscard]] int overlayCount() const;
  [[nodiscard]] int selectedOverlay() const;
  [[nodiscard]] QListWidget* overlayList() const noexcept;
  [[nodiscard]] QPushButton* loadButton() const noexcept;
  [[nodiscard]] QCheckBox* revealResponse() const noexcept;
  [[nodiscard]] QCheckBox* revealPoles() const noexcept;
  [[nodiscard]] QCheckBox* revealZeros() const noexcept;
  [[nodiscard]] QLabel* provenanceLabel() const noexcept;

 signals:
  void overlaySelected(int index);
  void overlayRemoved(int index);
  void loadRequested();
  void revealChanged(int index, bool response, bool poles, bool zeros);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  QList<Overlay> overlays_;
  int selected_{-1};
  bool populating_{false};
  QListWidget* list_{};
  QPushButton* load_{};
  QCheckBox* response_{};
  QCheckBox* poles_{};
  QCheckBox* zeros_{};
  QLabel* provenance_{};

  void showReveal();
  void emitReveal();
};
