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
  parser.addOption(body_option);
  parser.addOption(sample_rate_option);
  parser.addOption(shot_option);
  parser.addOption(shot_after_option);
  parser.addOption(target_option);
  parser.addOption(fit_option);
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
    if (parser.isSet(fit_option)) {
      QTimer::singleShot(0, &window, [&window] { window.startFit(); });
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
