#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;

class NumberBox final : public QWidget {
  Q_OBJECT

 public:
  explicit NumberBox(QWidget* parent = nullptr);

  void setRange(int minimum, int maximum);
  void setValue(int value);
  [[nodiscard]] int value() const noexcept { return value_; }
  [[nodiscard]] int minimum() const noexcept { return minimum_; }
  [[nodiscard]] int maximum() const noexcept { return maximum_; }
  void setText(const QString& text);
  [[nodiscard]] QString text() const { return text_; }
  void setFollowing(bool following);
  [[nodiscard]] bool following() const noexcept { return following_; }
  void type(const QString& entry);
  void openEditor();

  [[nodiscard]] QSize sizeHint() const override;

 signals:
  void valueChanged(int value);
  void typed(const QString& entry);
  void touched();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  void unfollow();
  void commitEditor();
  void closeEditor();

  QLineEdit* editor_{};
  QString text_;
  int minimum_{};
  int maximum_{255};
  int value_{};
  int press_y_{};
  int press_value_{};
  bool dragging_{};
  bool following_{};
};
