#include "section_model.hpp"

#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QColor>
#include <QComboBox>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

namespace p2k = trench::core::p2k;

constexpr std::size_t kRows = trench::core::kLegacySectionCount;

double scale_db(std::uint16_t word) {
  const double linear = trench::core::decode_word(word) * 4.0;
  return linear > 0.0 ? 20.0 * std::log10(linear) : -99.0;
}

}  // namespace

SectionModel::SectionModel(BodyDocument* document, QObject* parent)
    : QAbstractTableModel(parent), document_(document) {
  connect(document_, &BodyDocument::bodyChanged, this, [this] {
    emit dataChanged(index(0, 0), index(static_cast<int>(kRows) - 1, kColumnCount - 1));
  });
  connect(document_, &BodyDocument::cornerChanged, this, [this](std::size_t) {
    emit dataChanged(index(0, 0), index(static_cast<int>(kRows) - 1, kColumnCount - 1));
  });
  connect(document_, &BodyDocument::intentChanged, this, [this](std::size_t section) {
    const auto cell = index(static_cast<int>(section), kIntent);
    emit dataChanged(cell, cell);
  });
}

int SectionModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(kRows);
}

int SectionModel::columnCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : kColumnCount;
}

QString SectionModel::roleName(p2k::Role role) {
  switch (role) {
    case p2k::Role::kParked: return QStringLiteral("parked");
    case p2k::Role::kTilt: return QStringLiteral("tilt");
    case p2k::Role::kPeak: return QStringLiteral("peak");
    case p2k::Role::kNotch: return QStringLiteral("notch");
    case p2k::Role::kPeakNotch: return QStringLiteral("peak+notch");
    case p2k::Role::kRealAxis: return QStringLiteral("real");
  }
  return {};
}

QString SectionModel::intentName(std::optional<p2k::Role> intent) {
  return intent ? roleName(*intent) : QStringLiteral("free");
}

std::optional<p2k::Role> SectionModel::intentFromIndex(int index) {
  switch (index) {
    case 1: return p2k::Role::kTilt;
    case 2: return p2k::Role::kPeak;
    case 3: return p2k::Role::kNotch;
    case 4: return p2k::Role::kPeakNotch;
    case 5: return p2k::Role::kParked;
    default: return std::nullopt;
  }
}

int SectionModel::intentIndex(std::optional<p2k::Role> intent) {
  if (!intent) return 0;
  switch (*intent) {
    case p2k::Role::kTilt: return 1;
    case p2k::Role::kPeak: return 2;
    case p2k::Role::kNotch: return 3;
    case p2k::Role::kPeakNotch: return 4;
    case p2k::Role::kParked: return 5;
    default: return 0;
  }
}

QString SectionModel::typeName(p2k::SectionType type) {
  switch (type) {
    case p2k::SectionType::kOff: return QStringLiteral("off");
    case p2k::SectionType::kLowPass: return QStringLiteral("LP");
    case p2k::SectionType::kHighPass: return QStringLiteral("HP");
    case p2k::SectionType::kEq: return QStringLiteral("EQ");
  }
  return {};
}

p2k::SectionType SectionModel::typeFromIndex(int index) {
  switch (index) {
    case 1: return p2k::SectionType::kLowPass;
    case 2: return p2k::SectionType::kHighPass;
    case 3: return p2k::SectionType::kEq;
    default: return p2k::SectionType::kOff;
  }
}

int SectionModel::typeIndex(p2k::SectionType type) {
  switch (type) {
    case p2k::SectionType::kLowPass: return 1;
    case p2k::SectionType::kHighPass: return 2;
    case p2k::SectionType::kEq: return 3;
    case p2k::SectionType::kOff: return 0;
  }
  return 0;
}

QVariant SectionModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(kRows)) return {};
  const auto section = static_cast<std::size_t>(index.row());
  const auto& words = document_->body().words[document_->corner()][section];
  const auto param = p2k::param_of(words, trench::core::kP2kDatumHz);
  const auto derived = document_->roleOf(section);
  const auto intent = document_->intent()[section];

  if (role == Qt::ForegroundRole) {
    if (index.column() == kIntent && intent && !p2k::within_envelope(*intent, words)) {
      return QColor(226, 78, 74);
    }
    return {};
  }
  if (role == Qt::TextAlignmentRole) {
    return index.column() == kIntent || index.column() == kType
               ? QVariant(Qt::AlignLeft | Qt::AlignVCenter)
               : QVariant(Qt::AlignRight | Qt::AlignVCenter);
  }
  if (role != Qt::DisplayRole && role != Qt::EditRole) return {};

  const bool live = param.type != p2k::SectionType::kOff;
  switch (index.column()) {
    case kType:
      if (role == Qt::EditRole) return typeIndex(param.type);
      return typeName(param.type);
    case kFc:
      if (!live) return role == Qt::EditRole ? QVariant() : QVariant(QStringLiteral("—"));
      return role == Qt::EditRole
                 ? QVariant(param.fc_hz)
                 : QVariant(QString::number(param.fc_hz, 'f', param.fc_hz < 100.0 ? 1 : 0));
    case kBw:
      if (!live) return role == Qt::EditRole ? QVariant() : QVariant(QStringLiteral("—"));
      return role == Qt::EditRole ? QVariant(param.bw_oct)
                                  : QVariant(QString::number(param.bw_oct, 'f', 2));
    case kGain:
      if (!live) return role == Qt::EditRole ? QVariant() : QVariant(QStringLiteral("—"));
      return role == Qt::EditRole ? QVariant(param.gain_db)
                                  : QVariant(QString::number(param.gain_db, 'f', 1));
    case kIntent:
      if (role == Qt::EditRole) return intentIndex(intent);
      return intent ? intentName(intent) : roleName(derived);
    case kScale: {
      const double db = scale_db(words[4]);
      return role == Qt::EditRole ? QVariant(db) : QVariant(QString::number(db, 'f', 1));
    }
    default:
      return {};
  }
}

QVariant SectionModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role != Qt::DisplayRole) return {};
  if (orientation == Qt::Vertical) return QString::number(section + 1);
  switch (section) {
    case kType: return QStringLiteral("type");
    case kFc: return QStringLiteral("Fc");
    case kBw: return QStringLiteral("Bw");
    case kGain: return QStringLiteral("gain");
    case kIntent: return QStringLiteral("role");
    case kScale: return QStringLiteral("dB");
    default: return {};
  }
}

Qt::ItemFlags SectionModel::flags(const QModelIndex& index) const {
  auto f = QAbstractTableModel::flags(index);
  if (!index.isValid()) return f;
  const auto section = static_cast<std::size_t>(index.row());
  const auto& words = document_->body().words[document_->corner()][section];
  const auto param = p2k::param_of(words, trench::core::kP2kDatumHz);
  const bool live = param.type != p2k::SectionType::kOff;
  const bool editable = index.column() == kType || index.column() == kIntent ||
                        index.column() == kScale ||
                        ((index.column() == kFc || index.column() == kBw ||
                          index.column() == kGain) &&
                         live);
  return editable ? f | Qt::ItemIsEditable : f;
}

bool SectionModel::writeParam(std::size_t section, const p2k::SectionParam& param) {
  const auto before = document_->body().words[document_->corner()][section];
  const std::array<std::uint16_t, 4> current{before[0], before[1], before[2], before[3]};
  const auto roots = p2k::words_from_param(param, current, section, trench::core::kP2kDatumHz);
  auto candidate = before;
  for (std::size_t word = 0; word < roots.size(); ++word) {
    candidate[word] = roots[word];
  }
  if (candidate == before) return true;
  document_->applySection(section, candidate);
  document_->commitGesture(section, before);
  return true;
}

bool SectionModel::writeScale(std::size_t section, double db) {
  const auto before = document_->body().words[document_->corner()][section];
  auto candidate = before;
  candidate[4] = p2k::nearest_gain_word(std::pow(10.0, db / 20.0) / 4.0);
  if (candidate == before) return true;
  document_->applySection(section, candidate);
  document_->commitGesture(section, before);
  return true;
}

bool SectionModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid() || role != Qt::EditRole) return false;
  const auto section = static_cast<std::size_t>(index.row());
  const auto& words = document_->body().words[document_->corner()][section];
  auto param = p2k::param_of(words, trench::core::kP2kDatumHz);
  bool ok = false;
  switch (index.column()) {
    case kType: {
      const auto type = typeFromIndex(value.toInt());
      if (type == param.type) return true;
      if (param.type == p2k::SectionType::kOff) {
        param.fc_hz = param.fc_hz > 0.0 ? std::min(param.fc_hz, 8000.0) : 1000.0;
        param.bw_oct = 0.5;
        param.gain_db = 0.0;
      }
      param.type = type;
      return writeParam(section, param);
    }
    case kFc: {
      const double hz = value.toDouble(&ok);
      if (!ok || !(hz > 0.0)) return false;
      param.fc_hz = hz;
      return writeParam(section, param);
    }
    case kBw: {
      const double octaves = value.toDouble(&ok);
      if (!ok || !(octaves > 0.0)) return false;
      param.bw_oct = octaves;
      return writeParam(section, param);
    }
    case kGain: {
      const double db = value.toDouble(&ok);
      if (!ok) return false;
      param.gain_db = db;
      return writeParam(section, param);
    }
    case kIntent:
      document_->setIntent(section, intentFromIndex(value.toInt()));
      return true;
    case kScale: {
      const double db = value.toDouble(&ok);
      return ok && writeScale(section, db);
    }
    default:
      return false;
  }
}

QWidget* IntentDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                      const QModelIndex&) const {
  auto* combo = new QComboBox(parent);
  for (int i = 0; i < 6; ++i) {
    combo->addItem(SectionModel::intentName(SectionModel::intentFromIndex(i)));
  }
  return combo;
}

void IntentDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  if (auto* combo = qobject_cast<QComboBox*>(editor)) {
    combo->setCurrentIndex(index.data(Qt::EditRole).toInt());
  }
}

void IntentDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                  const QModelIndex& index) const {
  if (auto* combo = qobject_cast<QComboBox*>(editor)) {
    model->setData(index, combo->currentIndex(), Qt::EditRole);
  }
}

QWidget* TypeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                    const QModelIndex&) const {
  auto* combo = new QComboBox(parent);
  for (int i = 0; i < 4; ++i) {
    combo->addItem(SectionModel::typeName(SectionModel::typeFromIndex(i)));
  }
  return combo;
}

void TypeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  if (auto* combo = qobject_cast<QComboBox*>(editor)) {
    combo->setCurrentIndex(index.data(Qt::EditRole).toInt());
  }
}

void TypeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                const QModelIndex& index) const {
  if (auto* combo = qobject_cast<QComboBox*>(editor)) {
    model->setData(index, combo->currentIndex(), Qt::EditRole);
  }
}
