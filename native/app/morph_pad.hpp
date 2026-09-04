#pragma once

#include "editor_state.hpp"

#include <QWidget>

class QLineEdit;

class MorphPad final : public QWidget {
 public:
  explicit MorphPad(EditorState* state, QWidget* parent = nullptr);

  void setWorst(double morph, double q, double db);

  [[nodiscard]] QString morphLabel() const;
  [[nodiscard]] QString qLabel() const;

  void renameMorphAxis(const QString& name);
  void renameQAxis(const QString& name);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  enum class Axis { kNone, kMorph, kQ };

  [[nodiscard]] QRectF field() const;
  [[nodiscard]] QRectF cornerRect(std::size_t index) const;
  [[nodiscard]] QRectF morphNameRect() const;
  [[nodiscard]] QRectF qNameRect() const;
  [[nodiscard]] QPointF pointFor(double morph, double q) const;
  void trackTo(const QPointF& position);
  void openEditor(Axis axis);
  void commitEditor();
  void closeEditor();

  EditorState* state_{};
  QLineEdit* editor_{};
  Axis editing_{Axis::kNone};
  double worst_morph_{};
  double worst_q_{};
  double worst_db_{-1.0e9};
};
