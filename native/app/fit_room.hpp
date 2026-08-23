#pragma once

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstddef>
#include <utility>
#include <vector>

class QComboBox;
class QListWidget;
class QPushButton;

class FitRoom final : public QWidget {
  Q_OBJECT

 public:
  struct VowelGroup {
    QString type;
    QStringList names;
  };

  struct Overlay {
    QString name;
    std::vector<double> db;
    std::vector<double> marks_hz;
  };

  explicit FitRoom(QWidget* parent = nullptr);

  void setGridHz(std::vector<double> hz);
  void setResponse(std::vector<double> db);
  void setOverlays(QList<Overlay> overlays, int selected);
  void setScoreDb(double rms_db);
  void setVowels(QList<VowelGroup> groups);
  void setFitRunning(bool running);

  [[nodiscard]] int overlayCount() const;
  [[nodiscard]] int selectedOverlay() const;
  [[nodiscard]] std::size_t pointCount() const noexcept;
  [[nodiscard]] double differenceDbAt(std::size_t index) const;
  [[nodiscard]] double scoreDb() const noexcept;
  [[nodiscard]] QListWidget* overlayList() const noexcept;
  [[nodiscard]] QComboBox* vowelBox() const noexcept;
  [[nodiscard]] QPushButton* loadButton() const noexcept;

 signals:
  void overlaySelected(int index);
  void overlayRemoved(int index);
  void loadRequested();
  void vowelRequested(const QString& symbol);

 protected:
  void paintEvent(QPaintEvent* event) override;

  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  [[nodiscard]] QRectF plotRect() const;
  [[nodiscard]] std::pair<double, double> dbRange() const;

  std::vector<double> hz_;
  std::vector<double> response_db_;
  QList<Overlay> overlays_;
  int selected_{-1};
  double score_db_{0.0};
  bool fit_running_{false};
  bool populating_{false};
  QListWidget* list_{};
  QComboBox* vowels_{};
  QPushButton* load_{};
};
