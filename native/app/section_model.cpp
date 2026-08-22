#include "section_model.hpp"

#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QColor>
#include <QComboBox>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <variant>

namespace {

namespace p2k = trench::core::p2k;

constexpr std::size_t kRows = trench::core::kLegacySectionCount;

struct RootView {
  double hz{};
  double q{};
  bool conjugate{};
};

RootView root_view(const trench::core::RootPair& pair) {
  if (const auto* c = std::get_if<trench::core::ConjugatePair>(&pair)) {
    const double bw = -std::log(std::max(c->radius, 1e-9)) * trench::core::kP2kDatumHz /
                      std::numbers::pi;
    return {c->hz, bw > 0.0 ? c->hz / bw : 0.0, true};
  }
  return {};
}

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

QVariant SectionModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(kRows)) return {};
  const auto section = static_cast<std::size_t>(index.row());
  const auto& words = document_->body().words[document_->corner()][section];
  const auto geometry = trench::core::geometry_from_words(words, trench::core::kP2kDatumHz);
  const auto pole = root_view(geometry.pole);
  const auto zero = root_view(geometry.zero);
  const auto derived = document_->roleOf(section);
  const auto intent = document_->intent()[section];

  if (role == Qt::ForegroundRole) {
    if (index.column() == kIntent && intent && !p2k::within_envelope(*intent, words)) {
      return QColor(226, 78, 74);
    }
    return {};
  }
  if (role == Qt::TextAlignmentRole) {
    return index.column() == kIntent ? QVariant(Qt::AlignLeft | Qt::AlignVCenter)
                                     : QVariant(Qt::AlignRight | Qt::AlignVCenter);
  }
  if (role != Qt::DisplayRole && role != Qt::EditRole) return {};

  switch (index.column()) {
    case kIntent:
      if (role == Qt::EditRole) return intentIndex(intent);
      return intent ? intentName(intent) : roleName(derived);
    case kPoleHz:
      if (!pole.conjugate) return role == Qt::EditRole ? QVariant() : QVariant(QStringLiteral("—"));
      return role == Qt::EditRole ? QVariant(pole.hz) : QVariant(QString::number(pole.hz, 'f', pole.hz < 100.0 ? 1 : 0));
    case kPoleQ:
      if (!pole.conjugate) return role == Qt::EditRole ? QVariant() : QVariant(QStringLiteral("—"));
      return role == Qt::EditRole ? QVariant(pole.q) : QVariant(QString::number(pole.q, 'f', 1));
    case kZeroHz:
      if (!zero.conjugate) return role == Qt::EditRole ? QVariant() : QVariant(QStringLiteral("—"));
      return role == Qt::EditRole ? QVariant(zero.hz) : QVariant(QString::number(zero.hz, 'f', zero.hz < 100.0 ? 1 : 0));
    case kZeroQ:
      if (!zero.conjugate) return role == Qt::EditRole ? QVariant() : QVariant(QStringLiteral("—"));
      return role == Qt::EditRole ? QVariant(zero.q) : QVariant(QString::number(zero.q, 'f', 1));
    case kScaleDb: {
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
    case kIntent: return QStringLiteral("role");
    case kPoleHz: return QStringLiteral("pole Hz");
    case kPoleQ: return QStringLiteral("Q");
    case kZeroHz: return QStringLiteral("zero Hz");
    case kZeroQ: return QStringLiteral("Q");
    case kScaleDb: return QStringLiteral("dB");
    default: return {};
  }
}

Qt::ItemFlags SectionModel::flags(const QModelIndex& index) const {
  auto f = QAbstractTableModel::flags(index);
  if (!index.isValid()) return f;
  const auto section = static_cast<std::size_t>(index.row());
  const auto& words = document_->body().words[document_->corner()][section];
  const auto geometry = trench::core::geometry_from_words(words, trench::core::kP2kDatumHz);
  const bool pole_ok = std::holds_alternative<trench::core::ConjugatePair>(geometry.pole);
  const bool zero_ok = std::holds_alternative<trench::core::ConjugatePair>(geometry.zero);
  const bool editable = index.column() == kIntent || index.column() == kScaleDb ||
                        ((index.column() == kPoleHz || index.column() == kPoleQ) && pole_ok) ||
                        ((index.column() == kZeroHz || index.column() == kZeroQ) && zero_ok);
  return editable ? f | Qt::ItemIsEditable : f;
}

bool SectionModel::writeRoot(std::size_t section, bool pole, double hz, double q) {
  if (!(hz > 0.0) || !(q > 0.0)) return false;
  const auto before = document_->body().words[document_->corner()][section];
  double radius = std::exp(-std::numbers::pi * hz / (q * trench::core::kP2kDatumHz));
  radius = pole ? std::clamp(radius, 0.0, p2k::kPoleRMax) : std::clamp(radius, 0.0, 1.0);
  const double clamped_hz = std::clamp(hz, 20.0, p2k::kRootHiHz);
  const auto [word_mag, word_rsq] = p2k::words_from_root(clamped_hz, radius);
  const auto [p, qq] = p2k::pq(word_mag, word_rsq);
  if (!p2k::is_legal(p, qq, pole) ||
      !p2k::magnitude_admissible(p2k::nearest_lattice_word(word_mag), pole)) {
    return false;
  }
  auto candidate = before;
  candidate[pole ? 2 : 0] = word_mag;
  candidate[pole ? 3 : 1] = word_rsq;
  if (section == 5 && !pole) candidate[1] = p2k::kS6ZeroRsqWord;
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
  const auto geometry = trench::core::geometry_from_words(words, trench::core::kP2kDatumHz);
  const auto pole = root_view(geometry.pole);
  const auto zero = root_view(geometry.zero);
  bool ok = false;
  switch (index.column()) {
    case kIntent:
      document_->setIntent(section, intentFromIndex(value.toInt()));
      return true;
    case kPoleHz: {
      const double hz = value.toDouble(&ok);
      return ok && writeRoot(section, true, hz, pole.q);
    }
    case kPoleQ: {
      const double q = value.toDouble(&ok);
      return ok && writeRoot(section, true, pole.hz, q);
    }
    case kZeroHz: {
      const double hz = value.toDouble(&ok);
      return ok && writeRoot(section, false, hz, zero.q);
    }
    case kZeroQ: {
      const double q = value.toDouble(&ok);
      return ok && writeRoot(section, false, zero.hz, q);
    }
    case kScaleDb: {
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
