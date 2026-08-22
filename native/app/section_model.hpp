#pragma once

#include "body_document.hpp"
#include "trench/core/role.hpp"
#include "trench/core/section_param.hpp"

#include <QAbstractTableModel>
#include <QStyledItemDelegate>

#include <cstddef>
#include <optional>

class SectionModel final : public QAbstractTableModel {
  Q_OBJECT

 public:
  enum Column { kType = 0, kFc, kBw, kGain, kIntent, kScale, kColumnCount };

  explicit SectionModel(BodyDocument* document, QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role) const override;
  [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;

  static QString roleName(trench::core::p2k::Role role);
  static QString intentName(std::optional<trench::core::p2k::Role> intent);
  static std::optional<trench::core::p2k::Role> intentFromIndex(int index);
  static int intentIndex(std::optional<trench::core::p2k::Role> intent);

  static QString typeName(trench::core::p2k::SectionType type);
  static trench::core::p2k::SectionType typeFromIndex(int index);
  static int typeIndex(trench::core::p2k::SectionType type);

 private:
  [[nodiscard]] bool writeParam(std::size_t section,
                                const trench::core::p2k::SectionParam& param);
  [[nodiscard]] bool writeScale(std::size_t section, double db);

  BodyDocument* document_;
};

class IntentDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  using QStyledItemDelegate::QStyledItemDelegate;
  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
};

class TypeDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  using QStyledItemDelegate::QStyledItemDelegate;
  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
};
