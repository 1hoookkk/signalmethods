#include "fit_controller.hpp"
#include "main_window.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QMessageBox>
#include <QTimer>

#include <filesystem>
#include <stdexcept>

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
  QCommandLineOption saw_option(QStringLiteral("saw"),
                                QStringLiteral("Measure audio targets as a sawtooth source."));
  parser.addOption(saw_option);
  parser.addOption(save_option);
  parser.process(application);

  const auto default_body = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                            "ref/presets/P2k_013_talking_hedz.bin";
  const auto body_path = parser.isSet(body_option)
                             ? std::filesystem::path(parser.value(body_option).toStdWString())
                             : default_body;
  bool sample_rate_ok = false;
  const auto sample_rate_hz = parser.value(sample_rate_option).toDouble(&sample_rate_ok);
  if (!sample_rate_ok || sample_rate_hz <= 0.0) {
    parser.showHelp(2);
  }

  try {
    MainWindow window(body_path, sample_rate_hz);
    window.show();
    if (parser.isSet(target_option)) {
      window.loadTarget(
          std::filesystem::path(parser.value(target_option).toStdWString()));
    }
    window.setCorner(static_cast<std::size_t>(parser.value(corner_option).toUInt()));
    if (parser.isSet(saw_option)) {
      window.setSourceModel(trench::core::measure::Source::kSawtooth);
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
