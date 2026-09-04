#include "keyframe_grid.hpp"

#include "vowel_space.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCursor>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr QColor kPanel{255, 255, 255};
constexpr QColor kPanelEdge{200, 200, 200};
constexpr QColor kText{40, 40, 40};
constexpr QColor kMuted{140, 140, 140};
constexpr QColor kInk{0, 0, 0};
constexpr QColor kZeroLine{215, 215, 215};
constexpr QColor kMint{72, 190, 148};
constexpr QColor kCursor{196, 103, 79};

constexpr double kFrameDb = 30.0;

const QString kAll = QStringLiteral("ALL");
const QString kVowelSpace = QStringLiteral("VOWEL SPACE");
const QString kBank = QStringLiteral("BANK");
const QString kTypes = QStringLiteral("TYPES");

QString& groupMemory() {
  static QString held = kAll;
  return held;
}

QString& searchMemory() {
  static QString held;
  return held;
}

QString prettyBody(const QString& raw) {
  QString out = raw;
  out.replace(QLatin1Char('_'), QLatin1Char(' '));
  const QStringList words = out.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  QStringList made;
  made.reserve(words.size());
  for (const QString& word : words) {
    made.push_back(word.left(1).toUpper() + word.mid(1));
  }
  return made.join(QLatin1Char(' '));
}

std::pair<QString, QString> splitBodyCorner(const QString& raw) {
  const int cut = raw.indexOf(QLatin1Char(' '));
  if (cut <= 0) return {raw, QString()};
  return {raw.left(cut), raw.mid(cut + 1)};
}

QString tailOf(const QString& source) {
  const int slash = std::max(source.lastIndexOf(QLatin1Char('/')),
                             source.lastIndexOf(QLatin1Char('\\')));
  return slash >= 0 ? source.mid(slash + 1) : source;
}

}

class GridTile final : public QWidget {
 public:
  GridTile(KeyframeGrid* owner, int entry, QWidget* parent)
      : QWidget(parent), owner_(owner), entry_(entry) {
    setFixedSize(KeyframeGrid::kTileWidth, KeyframeGrid::kTileHeight);
    setMouseTracking(true);
  }

  void setTile(int tile) { tile_ = tile; }

 protected:
  void paintEvent(QPaintEvent*) override {
    const KeyframeGrid::Entry& entry = owner_->entries_[static_cast<std::size_t>(entry_)];
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    const QRectF body(0.5, 0.5, width() - 1.0, height() - 1.0);
    painter.fillRect(rect(), kPanel);
    const bool landed = owner_->landed_ == entry_;
    const bool cursor = owner_->cursor_ >= 0 && owner_->cursor_ == tile_;
    painter.setPen(QPen(landed ? kMint : (cursor ? kCursor : kPanelEdge), landed || cursor ? 2.0 : 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(body);

    QFont name_font = font();
    name_font.setPointSizeF(std::max(6.0, font().pointSizeF() - 1.0));
    painter.setFont(name_font);
    const QFontMetrics metrics(name_font);
    painter.setPen(kText);
    painter.drawText(QRect(5, 3, width() - 10, 13),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(entry.name, Qt::ElideRight, width() - 10));
    painter.setPen(kMuted);
    painter.drawText(QRect(5, 15, width() - 10, 12),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(entry.detail, Qt::ElideRight, width() - 10));

    const std::vector<double>& db = owner_->cached(static_cast<std::size_t>(entry_));
    const QRectF pane(5.0, 29.0, width() - 10.0, height() - 34.0);
    const double middle = pane.center().y();
    painter.setPen(QPen(kZeroLine, 1.0));
    painter.drawLine(QPointF(pane.left(), middle), QPointF(pane.right(), middle));
    if (db.size() < 2) return;
    QPainterPath path;
    const double span = pane.height() * 0.5;
    for (std::size_t i = 0; i < db.size(); ++i) {
      const double x = pane.left() + pane.width() * static_cast<double>(i) /
                                         static_cast<double>(db.size() - 1);
      const double level = std::clamp(db[i], -kFrameDb, kFrameDb);
      const double y = middle - span * level / kFrameDb;
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(kInk, 1.0));
    painter.drawPath(path);
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    owner_->land(tile_, (event->modifiers() & Qt::ShiftModifier) != 0);
  }

  void enterEvent(QEnterEvent* event) override {
    owner_->hoverTile(tile_);
    QWidget::enterEvent(event);
  }

 private:
  KeyframeGrid* owner_{};
  int entry_{-1};
  int tile_{-1};
};

const std::vector<trench::app::PoleTemplate>& KeyframeGrid::bank() {
  static const std::vector<trench::app::PoleTemplate> held = trench::app::loadFrames();
  return held;
}

const std::vector<trench::app::PoleTemplate>& KeyframeGrid::types() {
  static const std::vector<trench::app::PoleTemplate> held = trench::app::loadPoleTemplates();
  return held;
}

const std::vector<trench::app::Keyframe>& KeyframeGrid::keys() {
  static const std::vector<trench::app::Keyframe> held = trench::app::loadKeyframes();
  return held;
}

KeyframeGrid::KeyframeGrid(QWidget* parent) : QWidget(parent, Qt::Popup) {
  setObjectName(QStringLiteral("keyframeGrid"));
  setFocusPolicy(Qt::StrongFocus);
  setAutoFillBackground(true);
  setFixedSize(800, 440);

  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(8, 8, 8, 8);
  column->setSpacing(6);

  search_ = new QLineEdit(this);
  search_->setObjectName(QStringLiteral("keyframeSearch"));
  search_->setPlaceholderText(QStringLiteral("note, body, type"));
  search_->installEventFilter(this);
  column->addWidget(search_);

  auto* tab_row = new QHBoxLayout;
  tab_row->setSpacing(1);
  tabs_ = new QButtonGroup(this);
  tabs_->setExclusive(true);
  column->addLayout(tab_row);

  scroll_ = new QScrollArea(this);
  scroll_->setObjectName(QStringLiteral("keyframeScroll"));
  scroll_->setWidgetResizable(true);
  scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  board_ = new QWidget(scroll_);
  grid_ = new QGridLayout(board_);
  grid_->setContentsMargins(0, 0, 0, 0);
  grid_->setSpacing(4);
  grid_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  scroll_->setWidget(board_);
  column->addWidget(scroll_, 1);

  vowel_ = new VowelSpace(this);
  column->addWidget(vowel_, 1);
  vowel_->hide();
  vowel_->onHover = [this](const trench::app::Keyframe& key) {
    hover_->stop();
    pending_hover_ = -1;
    hovering_ = -1;
    hovering_frame_ = true;
    if (onHoverFrame) onHoverFrame(key);
  };
  vowel_->onHoverLeave = [this] { hoverLeave(); };
  vowel_->onPick = [this](const trench::app::Keyframe& key, bool partner) {
    if (!partner) hide();
    if (onPickFrame) onPickFrame(key, partner);
  };

  hover_ = new QTimer(this);
  hover_->setSingleShot(true);
  hover_->setInterval(kHoverMs);
  connect(hover_, &QTimer::timeout, this, [this] {
    if (pending_hover_ < 0 || pending_hover_ >= static_cast<int>(visible_.size())) return;
    hovering_ = pending_hover_;
    if (onHover) onHover(hovering_);
  });

  buildEntries();
  cache_.assign(entries_.size(), {});
  tiles_.reserve(entries_.size());
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    tiles_.push_back(new GridTile(this, static_cast<int>(i), board_));
    tiles_.back()->hide();
  }

  group_ = groupMemory();
  buildTabs();
  for (QAbstractButton* button : tabs_->buttons()) tab_row->addWidget(button);
  tab_row->addStretch(1);
  {
    const QSignalBlocker blocker(search_);
    search_->setText(searchMemory());
  }
  connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
    searchMemory() = text;
    cursor_ = -1;
    relayout();
  });
  relayout();
}

void KeyframeGrid::buildEntries() {
  const auto& corners = bank();
  for (std::size_t i = 0; i < corners.size(); ++i) {
    const auto split = splitBodyCorner(corners[i].type);
    Entry entry;
    entry.kind = Kind::kBank;
    entry.index = i;
    entry.group = kBank;
    entry.name = prettyBody(split.first) + QStringLiteral(" · ") + split.second;
    entry.detail = corners[i].family;
    entries_.push_back(std::move(entry));
  }
  const auto& roster = types();
  for (std::size_t i = 0; i < roster.size(); ++i) {
    Entry entry;
    entry.kind = Kind::kType;
    entry.index = i;
    entry.group = kTypes;
    entry.name = roster[i].family + QStringLiteral(" · ") + roster[i].type;
    entry.detail = roster[i].ladder
                       ? QStringLiteral("%1 rungs").arg(roster[i].ratios.size())
                       : QStringLiteral("%1 bodies").arg(roster[i].bodies.size());
    entries_.push_back(std::move(entry));
  }
  const auto& frames = keys();
  for (std::size_t i = 0; i < frames.size(); ++i) {
    Entry entry;
    entry.kind = Kind::kKeyframe;
    entry.index = i;
    entry.group = frames[i].group;
    entry.name = frames[i].name;
    entry.detail = frames[i].mode + QStringLiteral(" · ") + tailOf(frames[i].source);
    entries_.push_back(std::move(entry));
  }
  for (Entry& entry : entries_) {
    entry.needle = (entry.name + QLatin1Char(' ') + entry.detail + QLatin1Char(' ') + entry.group)
                       .toLower();
  }
}

void KeyframeGrid::buildTabs() {
  QStringList order{kBank, kTypes};
  QStringList names;
  std::vector<int> counts;
  for (const Entry& entry : entries_) {
    if (entry.kind != Kind::kKeyframe) continue;
    const int at = names.indexOf(entry.group);
    if (at < 0) {
      names.push_back(entry.group);
      counts.push_back(1);
    } else {
      ++counts[static_cast<std::size_t>(at)];
    }
  }
  std::vector<int> seats(names.size());
  for (std::size_t i = 0; i < seats.size(); ++i) seats[i] = static_cast<int>(i);
  std::stable_sort(seats.begin(), seats.end(), [&counts](int left, int right) {
    return counts[static_cast<std::size_t>(left)] > counts[static_cast<std::size_t>(right)];
  });
  for (const int seat : seats) order.push_back(names[seat]);
  order.push_back(kVowelSpace);
  order.push_back(kAll);

  if (!order.contains(group_)) group_ = kAll;
  for (const QString& name : order) {
    int count = 0;
    if (name == kVowelSpace) {
      count = vowel_ != nullptr ? vowel_->markCount() : 0;
    } else {
      for (const Entry& entry : entries_) {
        if (name == kAll || entry.group == name) ++count;
      }
    }
    const QString caption = QStringLiteral("%1   %2").arg(name).arg(count);
    auto* button = new QPushButton(caption, this);
    button->setCheckable(true);
    button->setFlat(true);
    button->setFocusPolicy(Qt::NoFocus);
    QFont tab_font = button->font();
    tab_font.setPointSizeF(std::max(6.0, button->font().pointSizeF() - 1.0));
    button->setFont(tab_font);
    button->setStyleSheet(QStringLiteral("QPushButton{padding:2px 3px;}"));
    button->setMinimumWidth(QFontMetrics(tab_font).horizontalAdvance(caption) + 12);
    button->setProperty("group", name);
    button->setChecked(name == group_);
    tabs_->addButton(button);
    connect(button, &QPushButton::clicked, this, [this, name] { setGroup(name); });
  }
}

void KeyframeGrid::relayout() {
  const bool vowel = group_ == kVowelSpace;
  if (vowel_ != nullptr) vowel_->setVisible(vowel);
  if (scroll_ != nullptr) scroll_->setVisible(!vowel);
  const QString needle = search_->text().trimmed().toLower();
  visible_.clear();
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const Entry& entry = entries_[i];
    if (group_ != kAll && entry.group != group_) continue;
    if (!needle.isEmpty() && !entry.needle.contains(needle)) continue;
    visible_.push_back(static_cast<int>(i));
  }
  if (vowel) visible_.clear();
  while (grid_->count() > 0) {
    QLayoutItem* item = grid_->takeAt(0);
    delete item;
  }
  for (GridTile* tile : tiles_) {
    tile->setTile(-1);
    tile->hide();
  }
  for (std::size_t i = 0; i < visible_.size(); ++i) {
    GridTile* tile = tiles_[static_cast<std::size_t>(visible_[i])];
    tile->setTile(static_cast<int>(i));
    grid_->addWidget(tile, static_cast<int>(i) / kColumns, static_cast<int>(i) % kColumns);
    tile->show();
  }
  if (cursor_ >= static_cast<int>(visible_.size())) cursor_ = -1;
  board_->adjustSize();
  update();
}

int KeyframeGrid::entryCount() const { return static_cast<int>(entries_.size()); }

int KeyframeGrid::tileCount() const { return static_cast<int>(visible_.size()); }

const KeyframeGrid::Entry* KeyframeGrid::entryAt(int tile) const {
  if (tile < 0 || tile >= static_cast<int>(visible_.size())) return nullptr;
  return &entries_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(tile)])];
}

QString KeyframeGrid::tileName(int tile) const {
  const Entry* entry = entryAt(tile);
  return entry != nullptr ? entry->name : QString();
}

QString KeyframeGrid::tileDetail(int tile) const {
  const Entry* entry = entryAt(tile);
  return entry != nullptr ? entry->detail : QString();
}

QString KeyframeGrid::tileGroup(int tile) const {
  const Entry* entry = entryAt(tile);
  return entry != nullptr ? entry->group : QString();
}

int KeyframeGrid::tileNamed(const QString& name) const {
  for (int tile = 0; tile < static_cast<int>(visible_.size()); ++tile) {
    if (tileName(tile) == name) return tile;
  }
  return -1;
}

QString KeyframeGrid::group() const { return group_; }

QString KeyframeGrid::searchText() const { return search_->text(); }

QStringList KeyframeGrid::groups() const {
  QStringList out;
  for (QAbstractButton* button : tabs_->buttons()) {
    out.push_back(button->property("group").toString());
  }
  return out;
}

int KeyframeGrid::cursorTile() const { return cursor_; }

int KeyframeGrid::landedEntry() const { return landed_; }

const std::vector<double>& KeyframeGrid::sparklineHz() const {
  return trench::app::responseGridHz();
}

const std::vector<double>& KeyframeGrid::cached(std::size_t entry) const {
  static const std::vector<double> nothing;
  if (entry >= cache_.size()) return nothing;
  if (cache_[entry].empty()) {
    const Entry& which = entries_[entry];
    switch (which.kind) {
      case Kind::kBank:
        cache_[entry] = trench::app::responseDb(bank()[which.index], trench::app::responseGridHz());
        break;
      case Kind::kType:
        cache_[entry] = trench::app::responseDb(types()[which.index], trench::app::responseGridHz());
        break;
      case Kind::kKeyframe:
        cache_[entry] = trench::app::responseDb(keys()[which.index], trench::app::responseGridHz());
        break;
    }
  }
  return cache_[entry];
}

const std::vector<double>& KeyframeGrid::tileResponseDb(int tile) const {
  static const std::vector<double> nothing;
  if (tile < 0 || tile >= static_cast<int>(visible_.size())) return nothing;
  return cached(static_cast<std::size_t>(visible_[static_cast<std::size_t>(tile)]));
}

std::vector<double> KeyframeGrid::responseOn(int tile, const std::vector<double>& hz) const {
  const Entry* entry = entryAt(tile);
  if (entry == nullptr) return {};
  switch (entry->kind) {
    case Kind::kBank:
      return trench::app::responseDb(bank()[entry->index], hz);
    case Kind::kType:
      return trench::app::responseDb(types()[entry->index], hz);
    case Kind::kKeyframe:
      return trench::app::responseDb(keys()[entry->index], hz);
  }
  return {};
}

void KeyframeGrid::setGroup(const QString& name) {
  if (!groups().contains(name)) return;
  group_ = name;
  groupMemory() = name;
  for (QAbstractButton* button : tabs_->buttons()) {
    const QSignalBlocker blocker(button);
    button->setChecked(button->property("group").toString() == name);
  }
  cursor_ = -1;
  relayout();
}

void KeyframeGrid::setSearch(const QString& text) {
  search_->setText(text);
}

void KeyframeGrid::pickTile(int tile) { land(tile, false); }

void KeyframeGrid::hoverTileForTest(int tile) {
  if (tile < 0 || tile >= static_cast<int>(visible_.size())) return;
  hover_->stop();
  pending_hover_ = tile;
  hovering_ = tile;
  if (onHover) onHover(hovering_);
}

void KeyframeGrid::hoverLeaveForTest() { hoverLeave(); }

void KeyframeGrid::pickTileOnPartner(int tile) { land(tile, true); }

void KeyframeGrid::land(int tile, bool partner) {
  const Entry* entry = entryAt(tile);
  if (entry == nullptr) return;
  landed_ = visible_[static_cast<std::size_t>(tile)];
  if (partner) {
    if (onPick) onPick(*entry, true);
    update();
    for (GridTile* one : tiles_) one->update();
    return;
  }
  const Entry held = *entry;
  hide();
  if (onPick) onPick(held, false);
}

void KeyframeGrid::moveCursor(int delta) {
  if (visible_.empty()) return;
  const int last = static_cast<int>(visible_.size()) - 1;
  cursor_ = std::clamp(cursor_ < 0 ? 0 : cursor_ + delta, 0, last);
  for (GridTile* one : tiles_) one->update();
  if (cursor_ >= 0 && cursor_ < static_cast<int>(visible_.size())) {
    scroll_->ensureWidgetVisible(tiles_[static_cast<std::size_t>(visible_[static_cast<std::size_t>(cursor_)])]);
  }
}

void KeyframeGrid::hoverTile(int tile) {
  if (tile < 0) return;
  pending_hover_ = tile;
  hover_->start();
}

void KeyframeGrid::hoverLeave() {
  hover_->stop();
  pending_hover_ = -1;
  if (hovering_ < 0 && !hovering_frame_) return;
  hovering_ = -1;
  hovering_frame_ = false;
  if (onHoverLeave) onHoverLeave();
}

bool KeyframeGrid::eventFilter(QObject* watched, QEvent* event) {
  if (watched == search_ && event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    switch (key->key()) {
      case Qt::Key_Down:
        moveCursor(1);
        return true;
      case Qt::Key_Up:
        moveCursor(-1);
        return true;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        if (cursor_ >= 0) land(cursor_, (key->modifiers() & Qt::ShiftModifier) != 0);
        return true;
      default:
        break;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void KeyframeGrid::leaveEvent(QEvent* event) {
  if (!rect().contains(mapFromGlobal(QCursor::pos()))) hoverLeave();
  QWidget::leaveEvent(event);
}

void KeyframeGrid::hideEvent(QHideEvent* event) {
  hoverLeave();
  if (onClose) onClose();
  QWidget::hideEvent(event);
}
