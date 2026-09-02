#include "editor_state.hpp"
#include "main_window.hpp"
#include "skin.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <span>

int main(int argc, char* argv[]) {
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
  QApplication application(argc, argv);
  trench::app::applyNinetiesSkin(application);
  application.setApplicationName(QStringLiteral("TRENCH"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Six-section direct root editor"));
  parser.addHelpOption();
  QCommandLineOption audition_option(
      QStringLiteral("audition"),
      QStringLiteral("Open audition audio on startup."));
  QCommandLineOption open_option(
      QStringLiteral("open"),
      QStringLiteral("Body to open on startup."),
      QStringLiteral("path"));
  QCommandLineOption pad_option(
      QStringLiteral("pad"),
      QStringLiteral("Morph and Q pad position as m,q in 0..1."),
      QStringLiteral("m,q"));
  QCommandLineOption dump_option(
      QStringLiteral("dump"),
      QStringLiteral("Write the cascade response at the pad to a CSV and exit."),
      QStringLiteral("path"));
  QCommandLineOption capture_option(
      QStringLiteral("capture"),
      QStringLiteral("Grab the window to a PNG and exit."),
      QStringLiteral("path"));
  parser.addOption(audition_option);
  parser.addOption(open_option);
  parser.addOption(pad_option);
  parser.addOption(dump_option);
  parser.addOption(capture_option);
  parser.process(application);

  MainWindow window;
  const QStringList opens = parser.values(open_option);
  const QStringList pads = parser.values(pad_option);
  for (qsizetype index = 0; index < std::max(opens.size(), pads.size()); ++index) {
    if (index < opens.size()) window.openPath(opens[index]);
    if (index < pads.size()) {
      const QStringList parts = pads[index].split(QLatin1Char(','));
      if (parts.size() == 2) {
        window.state().setPadPosition(parts[0].toDouble(), parts[1].toDouble());
      }
    }
  }
  if (parser.isSet(dump_option)) {
    const auto cascade = window.state().cascade();
    const std::span<const trench::core::Biquad> six{cascade.data(), trench::core::native::kSections};
    std::ofstream out(std::filesystem::path(parser.value(dump_option).toStdWString()));
    for (const double hz : trench::core::logarithmic_frequency_grid(40.0, 16000.0, 512)) {
      out << hz << ',' << trench::core::cascade_response_db(six, hz, EditorState::kDatumHz) << '\n';
    }
    return 0;
  }
  window.show();
  if (parser.isSet(capture_option)) {
    QTimer::singleShot(250, &window, [&] {
      window.grab().save(parser.value(capture_option));
      application.quit();
    });
  }
  if (parser.isSet(audition_option)) {
    QTimer::singleShot(0, &window, [&window] { window.setAudition(true); });
  }
  return application.exec();
}
