#include "main_window.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QTimer>

#include <filesystem>

int main(int argc, char* argv[]) {
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("TRENCH"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Six-section direct root editor"));
  parser.addHelpOption();
  QCommandLineOption reference_option(
      {QStringLiteral("r"), QStringLiteral("reference")},
      QStringLiteral("Response, audio, or packed reference to overlay."),
      QStringLiteral("path"));
  QCommandLineOption audition_option(
      QStringLiteral("audition"),
      QStringLiteral("Open audition audio on startup."));
  parser.addOption(reference_option);
  parser.addOption(audition_option);
  parser.process(application);

  MainWindow window;
  if (parser.isSet(reference_option)) {
    const auto path = std::filesystem::path(
        parser.value(reference_option).toStdWString());
    if (!window.loadReference(path)) return 2;
  }
  window.show();
  if (parser.isSet(audition_option)) {
    QTimer::singleShot(0, &window, [&window] { window.setAudition(true); });
  }
  return application.exec();
}
