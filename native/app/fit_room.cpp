#include "fit_room.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

FitRoom::FitRoom(QWidget* parent) : QWidget(parent) {
  setWindowFlags(Qt::Tool);
  setWindowTitle(QStringLiteral("TARGETS"));
  setMinimumSize(320, 220);
  resize(360, 280);
  setStyleSheet(QStringLiteral(
      "FitRoom { background: #111416; }"
      "QListWidget, QPushButton { background: #1a1f23; color: #aebabe; "
      "border: 1px solid #373f43; border-radius: 2px; padding: 3px 6px; }"
      "QListWidget::item:selected { background: #263238; color: #57decd; }"));

  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(8, 8, 8, 8);
  column->setSpacing(6);

  list_ = new QListWidget(this);
  list_->setObjectName(QStringLiteral("overlayList"));
  list_->setAccessibleName(QStringLiteral("Fit targets"));
  list_->installEventFilter(this);
  column->addWidget(list_, 1);

  auto* row = new QHBoxLayout();
  row->addStretch(1);
  load_ = new QPushButton(QStringLiteral("LOAD"), this);
  load_->setObjectName(QStringLiteral("loadOverlay"));
  load_->setAccessibleName(QStringLiteral("Load target"));
  load_->setFixedWidth(72);
  row->addWidget(load_);
  column->addLayout(row);

  connect(list_, &QListWidget::currentRowChanged, this, [this](int index) {
    if (populating_) return;
    selected_ = index;
    emit overlaySelected(index);
  });
  connect(load_, &QPushButton::clicked, this, [this] { emit loadRequested(); });
}

void FitRoom::setOverlays(QList<Overlay> overlays, int selected) {
  overlays_ = std::move(overlays);
  selected_ = selected;
  populating_ = true;
  list_->clear();
  for (const auto& overlay : overlays_) list_->addItem(overlay.name);
  list_->setCurrentRow(selected);
  populating_ = false;
}

int FitRoom::overlayCount() const { return static_cast<int>(overlays_.size()); }

int FitRoom::selectedOverlay() const { return selected_; }

QListWidget* FitRoom::overlayList() const noexcept { return list_; }

QPushButton* FitRoom::loadButton() const noexcept { return load_; }

bool FitRoom::eventFilter(QObject* watched, QEvent* event) {
  if (watched == list_ && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Delete) {
      emit overlayRemoved(list_->currentRow());
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}
