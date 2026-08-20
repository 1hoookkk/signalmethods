#include "main_window.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QMessageBox>

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
  parser.addOption(body_option);
  parser.addOption(sample_rate_option);
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
    return application.exec();
  } catch (const std::exception& error) {
    QMessageBox::critical(nullptr, QStringLiteral("TRENCH"),
                          QString::fromUtf8(error.what()));
    return 1;
  }
}
