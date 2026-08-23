#include "bisect_room.hpp"
#include "body_document.hpp"
#include "fit_controller.hpp"
#include "main_window.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/role.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QMessageBox>
#include <QString>
#include <QElapsedTimer>
#include <QTimer>

#include <cmath>
#include <cstdio>

#include <filesystem>
#include <optional>
#include <stdexcept>

namespace {

namespace p2k = trench::core::p2k;

std::optional<p2k::Role> role_from_name(const QString& name, bool& ok) {
  ok = true;
  if (name == QLatin1String("free")) return std::nullopt;
  if (name == QLatin1String("tilt")) return p2k::Role::kTilt;
  if (name == QLatin1String("peak")) return p2k::Role::kPeak;
  if (name == QLatin1String("notch")) return p2k::Role::kNotch;
  if (name == QLatin1String("peak+notch")) return p2k::Role::kPeakNotch;
  if (name == QLatin1String("parked")) return p2k::Role::kParked;
  ok = false;
  return std::nullopt;
}

bool apply_intent(BodyDocument* document, const QString& spec) {
  for (const auto& entry : spec.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    const auto parts = entry.split(QLatin1Char('='));
    if (parts.size() != 2) return false;
    bool row_ok = false;
    const auto row = parts[0].trimmed().toUInt(&row_ok);
    if (!row_ok || row < 1 || row > trench::core::kLegacySectionCount) return false;
    bool role_ok = false;
    const auto role = role_from_name(parts[1].trimmed().toLower(), role_ok);
    if (!role_ok) return false;
    document->setIntent(row - 1, role);
  }
  return true;
}

bool apply_space(BodyDocument* document, const QString& spec) {
  const auto parts = spec.split(QLatin1Char(','), Qt::SkipEmptyParts);
  if (parts.size() < 2 || parts.size() > 4) return false;
  auto space = document->space();
  bool ok = false;
  space.lo_hz = parts[0].trimmed().toDouble(&ok);
  if (!ok) return false;
  space.hi_hz = parts[1].trimmed().toDouble(&ok);
  if (!ok || space.hi_hz <= space.lo_hz) return false;
  if (parts.size() > 2) {
    const auto weight = parts[2].trimmed().toLower();
    if (weight == QLatin1String("erb")) {
      space.weight = p2k::PerceptualSpace::Weight::kErb;
    } else if (weight == QLatin1String("flat")) {
      space.weight = p2k::PerceptualSpace::Weight::kFlat;
    } else {
      return false;
    }
  }
  if (parts.size() > 3) {
    space.smooth_octaves = parts[3].trimmed().toDouble(&ok);
    if (!ok || space.smooth_octaves < 0.0) return false;
  }
  document->setSpace(space);
  return true;
}

bool apply_view(BodyDocument* document, const QString& spec) {
  const auto parts = spec.split(QLatin1Char(','), Qt::SkipEmptyParts);
  if (parts.size() != 2) return false;
  bool morph_ok = false;
  bool q_ok = false;
  const auto morph = parts[0].trimmed().toFloat(&morph_ok);
  const auto q = parts[1].trimmed().toFloat(&q_ok);
  if (!morph_ok || !q_ok) return false;
  document->setView(morph, q);
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("TRENCH"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("TRENCH native response surface"));
  parser.addHelpOption();
  QCommandLineOption body_option({QStringLiteral("b"), QStringLiteral("body")},
                                 QStringLiteral("Packed body to display."),
                                 QStringLiteral("path"));
  QCommandLineOption sample_rate_option(
      {QStringLiteral("r"), QStringLiteral("sample-rate")},
      QStringLiteral("Authoring datum in Hz."), QStringLiteral("hz"),
      QStringLiteral("44100"));
  QCommandLineOption shot_option(QStringLiteral("shot"),
                                 QStringLiteral("Grab the window to a file and quit."),
                                 QStringLiteral("path"));
  QCommandLineOption shot_after_option(QStringLiteral("shot-after"),
                                       QStringLiteral("Delay before the grab in ms."),
                                       QStringLiteral("ms"), QStringLiteral("0"));
  QCommandLineOption target_option(QStringLiteral("target"),
                                   QStringLiteral("Load a fit target on startup."),
                                   QStringLiteral("path"));
  QCommandLineOption fit_option(QStringLiteral("fit"),
                                QStringLiteral("Start FIT after showing."));
  QCommandLineOption corner_option(QStringLiteral("corner"),
                                   QStringLiteral("Select a corner 0-3 on startup."),
                                   QStringLiteral("index"), QStringLiteral("0"));
  QCommandLineOption save_option(QStringLiteral("save"),
                                 QStringLiteral("Save the body here after FIT ends (or at once) and quit."),
                                 QStringLiteral("path"));
  parser.addOption(body_option);
  parser.addOption(sample_rate_option);
  parser.addOption(shot_option);
  parser.addOption(shot_after_option);
  parser.addOption(target_option);
  parser.addOption(fit_option);
  parser.addOption(corner_option);
  QCommandLineOption audition_option(QStringLiteral("audition"),
                                     QStringLiteral("Open the gate, sweep Morph 0-1-0 over this many seconds, then quit."),
                                     QStringLiteral("seconds"));
  parser.addOption(audition_option);
  QCommandLineOption load_corner_option(QStringLiteral("load-corner"),
                                        QStringLiteral("Load a .corner file into a slot, \"N=path\" with N in 1-4; repeatable."),
                                        QStringLiteral("spec"));
  parser.addOption(load_corner_option);
  QCommandLineOption saw_option(QStringLiteral("saw"),
                                QStringLiteral("Measure audio targets as a sawtooth source."));
  QCommandLineOption intent_option(
      QStringLiteral("intent"),
      QStringLiteral("Row intents, \"1=tilt,2=peak,...\" over rows 1-6 and "
                     "tilt|peak|notch|peak+notch|parked|free."),
      QStringLiteral("spec"));
  QCommandLineOption space_option(
      QStringLiteral("space"),
      QStringLiteral("Perceptual space, \"lo,hi[,erb|flat[,smooth_oct]]\"."),
      QStringLiteral("spec"));
  QCommandLineOption view_option(QStringLiteral("view"),
                                 QStringLiteral("Interior view \"morph,q\" after showing."),
                                 QStringLiteral("spec"));
  parser.addOption(saw_option);
  parser.addOption(intent_option);
  parser.addOption(space_option);
  parser.addOption(view_option);
  QCommandLineOption bisect_option(QStringLiteral("bisect"),
                                   QStringLiteral("Eyes-closed bisection session on morph|q|character."),
                                   QStringLiteral("axis"));
  parser.addOption(save_option);
  parser.addOption(bisect_option);
  parser.process(application);

  const auto body_path = parser.isSet(body_option)
                             ? std::filesystem::path(parser.value(body_option).toStdWString())
                             : std::filesystem::path();
  bool sample_rate_ok = false;
  const auto sample_rate_hz = parser.value(sample_rate_option).toDouble(&sample_rate_ok);
  if (!sample_rate_ok || sample_rate_hz <= 0.0) {
    parser.showHelp(2);
  }

  try {
    MainWindow window(body_path, sample_rate_hz);
    if (parser.isSet(bisect_option)) {
      const auto axis = BisectRoom::axisFromName(parser.value(bisect_option).trimmed().toLower());
      if (!axis || body_path.empty()) parser.showHelp(2);
      auto* room = new BisectRoom(&window, *axis, body_path);
      room->show();
      room->setFocus();
      if (parser.isSet(shot_option)) {
        const auto shot_path = parser.value(shot_option);
        QTimer::singleShot(parser.value(shot_after_option).toInt(), room, [room, shot_path] {
          room->grab().save(shot_path);
          QCoreApplication::quit();
        });
      }
      return application.exec();
    }
    window.show();
    window.setCorner(static_cast<std::size_t>(parser.value(corner_option).toUInt()));
    if (parser.isSet(saw_option)) {
      window.setSourceModel(trench::core::measure::Source::kSawtooth);
    }
    if (parser.isSet(space_option) &&
        !apply_space(window.document(), parser.value(space_option))) {
      parser.showHelp(2);
    }
    if (parser.isSet(intent_option) &&
        !apply_intent(window.document(), parser.value(intent_option))) {
      parser.showHelp(2);
    }
    if (parser.isSet(target_option)) {
      window.loadTarget(
          std::filesystem::path(parser.value(target_option).toStdWString()));
    }
    if (parser.isSet(view_option) &&
        !apply_view(window.document(), parser.value(view_option))) {
      parser.showHelp(2);
    }
    const auto save_path = parser.isSet(save_option)
                               ? std::filesystem::path(parser.value(save_option).toStdWString())
                               : std::filesystem::path();
    auto save_and_quit = [&window, save_path] {
      const auto ok = window.saveBody(save_path);
      QCoreApplication::exit(ok ? 0 : 3);
    };
    if (parser.isSet(fit_option)) {
      if (!save_path.empty()) {
        QObject::connect(window.fitController(), &FitController::finished, &window,
                         [save_and_quit](quint64, bool, const QList<quint16>&) {
                           QTimer::singleShot(0, save_and_quit);
                         });
      }
      QTimer::singleShot(0, &window, [&window] { window.startFit(); });
    } else if (!save_path.empty()) {
      QTimer::singleShot(0, &window, save_and_quit);
    }
    for (const auto& spec : parser.values(load_corner_option)) {
      const auto eq = spec.indexOf(QLatin1Char('='));
      if (eq <= 0) return 4;
      const auto slot = spec.left(eq).toUInt();
      if (slot < 1 || slot > trench::core::kLegacyCornerCount) return 4;
      window.setCorner(slot - 1);
      if (!window.loadCorner(std::filesystem::path(spec.mid(eq + 1).toStdWString()))) return 4;
    }
    if (parser.isSet(audition_option)) {
      const auto seconds = std::max(parser.value(audition_option).toDouble(), 0.5);
      auto* sweep = new QTimer(&window);
      auto* clock = new QElapsedTimer();
      QObject::connect(sweep, &QTimer::timeout, &window, [&window, sweep, clock, seconds] {
        const double t = static_cast<double>(clock->elapsed()) / 1000.0;
        if (t >= seconds) {
          window.setAuditionGate(false);
          sweep->stop();
          delete clock;
          QTimer::singleShot(300, &QCoreApplication::quit);
          return;
        }
        const double phase = t / seconds;
        window.document()->setView(static_cast<float>(1.0 - std::abs(2.0 * phase - 1.0)), 0.0F);
      });
      QTimer::singleShot(0, &window, [&window, sweep, clock] {
        window.setAuditionGate(true);
        std::fprintf(stderr, "audition %s\n", window.auditionOpen() ? "open" : "no device");
        if (!window.auditionOpen()) QCoreApplication::exit(2);
        clock->start();
        sweep->start(10);
      });
    }
    if (parser.isSet(shot_option)) {
      const auto shot_path = parser.value(shot_option);
      const auto delay_ms = parser.value(shot_after_option).toInt();
      QTimer::singleShot(delay_ms, &window, [&window, shot_path] {
        window.grab().save(shot_path);
        QCoreApplication::quit();
      });
    }
    return application.exec();
  } catch (const std::exception& error) {
    QMessageBox::critical(nullptr, QStringLiteral("TRENCH"),
                          QString::fromUtf8(error.what()));
    return 1;
  }
}
