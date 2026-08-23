#include "posture_list.hpp"

#include <QFont>

namespace {

const QColor kChassis{26, 31, 35};
const QColor kInk{174, 186, 190};
const QColor kDim{125, 136, 140};
const QColor kMark{87, 222, 205};
const QColor kMarkTint{87, 222, 205, 36};

constexpr int kListWidthPx = 150;

}  // namespace

PostureList::PostureList(QWidget* parent) : QListWidget(parent) {
  setObjectName(QStringLiteral("postureList"));
  setFocusPolicy(Qt::NoFocus);
  setSelectionMode(QAbstractItemView::NoSelection);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFrameShape(QFrame::NoFrame);
  setUniformItemSizes(true);
  setFixedWidth(kListWidthPx);
  setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::DemiBold));
  setStyleSheet(QStringLiteral(
      "QListWidget { background: %1; border: none; border-left: 1px solid #373f43; "
      "outline: none; }"
      "QListWidget::item { padding: 1px 6px; }"
      "QScrollBar:vertical { background: %1; width: 6px; margin: 0; }"
      "QScrollBar::handle:vertical { background: #373f43; border-radius: 3px; }"
      "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }"
      "QScrollBar::add-page, QScrollBar::sub-page { background: %1; }")
                    .arg(kChassis.name()));

  connect(this, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    const auto name = item->data(Qt::UserRole).toString();
    if (name.isEmpty()) return;
    emit postureChosen(name);
  });
}

void PostureList::setGroups(QList<Group> groups) {
  clear();
  for (const auto& group : groups) {
    if (group.names.isEmpty()) continue;
    auto* header = new QListWidgetItem(group.type, this);
    header->setFlags(Qt::NoItemFlags);
    header->setForeground(kDim);
    for (const auto& name : group.names) {
      auto* item = new QListWidgetItem(name, this);
      item->setData(Qt::UserRole, name);
    }
  }
  paintRows();
}

void PostureList::setMatched(const QString& name) {
  if (matched_ == name) return;
  matched_ = name;
  paintRows();
}

QString PostureList::matched() const { return matched_; }

int PostureList::rowOf(const QString& name) const {
  for (int row = 0; row < count(); ++row) {
    if (item(row)->data(Qt::UserRole).toString() == name) return row;
  }
  return -1;
}

void PostureList::paintRows() {
  for (int row = 0; row < count(); ++row) {
    auto* entry = item(row);
    const auto name = entry->data(Qt::UserRole).toString();
    if (name.isEmpty()) continue;
    const bool marked = !matched_.isEmpty() && name == matched_;
    entry->setForeground(marked ? kMark : kInk);
    entry->setBackground(marked ? QBrush(kMarkTint) : QBrush(Qt::NoBrush));
  }
}
