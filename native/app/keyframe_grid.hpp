#pragma once

#include "pole_templates.hpp"

#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <vector>

class QButtonGroup;
class QEvent;
class QGridLayout;
class QHideEvent;
class QLineEdit;
class QScrollArea;
class QTimer;
class GridTile;
class VowelSpace;

class KeyframeGrid final : public QWidget {
 public:
  enum class Kind { kBank, kType, kKeyframe };

  struct Entry {
    Kind kind{Kind::kBank};
    std::size_t index{};
    QString group;
    QString name;
    QString detail;
    QString needle;
  };

  static constexpr int kColumns = 5;
  static constexpr int kTileWidth = 132;
  static constexpr int kTileHeight = 64;
  static constexpr int kHoverMs = 400;

  explicit KeyframeGrid(QWidget* parent = nullptr);

  [[nodiscard]] static const std::vector<trench::app::PoleTemplate>& bank();
  [[nodiscard]] static const std::vector<trench::app::PoleTemplate>& types();
  [[nodiscard]] static const std::vector<trench::app::Keyframe>& keys();

  [[nodiscard]] int entryCount() const;
  [[nodiscard]] int tileCount() const;
  [[nodiscard]] QString tileName(int tile) const;
  [[nodiscard]] QString tileDetail(int tile) const;
  [[nodiscard]] QString tileGroup(int tile) const;
  [[nodiscard]] int tileNamed(const QString& name) const;
  [[nodiscard]] QString group() const;
  [[nodiscard]] QString searchText() const;
  [[nodiscard]] QStringList groups() const;
  [[nodiscard]] int cursorTile() const;
  [[nodiscard]] int landedEntry() const;
  [[nodiscard]] const std::vector<double>& sparklineHz() const;
  [[nodiscard]] const std::vector<double>& tileResponseDb(int tile) const;
  [[nodiscard]] std::vector<double> responseOn(int tile, const std::vector<double>& hz) const;
  [[nodiscard]] const Entry* entryAt(int tile) const;
  [[nodiscard]] VowelSpace* vowelSpace() const noexcept { return vowel_; }

  void setGroup(const QString& name);
  void setSearch(const QString& text);
  void pickTile(int tile);
  void pickTileOnPartner(int tile);
  void hoverTileForTest(int tile);
  void hoverLeaveForTest();

  std::function<void(const Entry&, bool)> onPick;
  std::function<void(const trench::app::Keyframe&, bool)> onPickFrame;
  std::function<void(int)> onHover;
  std::function<void(const trench::app::Keyframe&)> onHoverFrame;
  std::function<void()> onHoverLeave;
  std::function<void()> onClose;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void hideEvent(QHideEvent* event) override;

 private:
  friend class GridTile;

  void buildEntries();
  void buildTabs();
  void relayout();
  void land(int tile, bool partner);
  void moveCursor(int delta);
  void hoverTile(int tile);
  void hoverLeave();
  [[nodiscard]] const std::vector<double>& cached(std::size_t entry) const;

  std::vector<Entry> entries_;
  std::vector<int> visible_;
  std::vector<GridTile*> tiles_;
  mutable std::vector<std::vector<double>> cache_;
  QLineEdit* search_{};
  QButtonGroup* tabs_{};
  QScrollArea* scroll_{};
  QWidget* board_{};
  VowelSpace* vowel_{};
  QGridLayout* grid_{};
  QTimer* hover_{};
  QString group_;
  int cursor_{-1};
  int landed_{-1};
  int pending_hover_{-1};
  int hovering_{-1};
  bool hovering_frame_{};
};
