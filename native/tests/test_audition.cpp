#include "harness.hpp"

#include "editor_state.hpp"
#include "main_window.hpp"
#include "trench/audio/audition.hpp"

#include <QLabel>
#include <QPushButton>
#include <QTest>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr int kNoDeviceExit = 77;

void requireAudible() {
  const char* audible = std::getenv("TRENCH_AUDIBLE");
  if (audible == nullptr || *audible == '\0') {
    std::printf("SKIP set TRENCH_AUDIBLE=1 to run the audible smoke\n");
    std::exit(kNoDeviceExit);
  }
}

}  // namespace

TRENCH_TEST(audition_device_opens_streams_and_closes) {
  requireAudible();
  trench::audio::Audition audition;
  const std::string error = audition.start();
  if (!error.empty()) {
    std::printf("SKIP no audio device: %s\n", error.c_str());
    std::exit(kNoDeviceExit);
  }
  CHECK(audition.running());
  CHECK(audition.sampleRateHz() > 0.0);
  std::printf("device \"%s\" at %.0f Hz\n", audition.deviceName().c_str(), audition.sampleRateHz());
  EditorState state;
  audition.setView(state.view());
  audition.setSaw(73.42, 0.18F);
  audition.setGate(true);
  QTest::qWait(1500);
  audition.setGate(false);
  QTest::qWait(200);
  audition.stop();
  CHECK(!audition.running());
  CHECK(audition.deviceName().empty());
  CHECK(audition.start().empty());
  CHECK(audition.running());
  audition.stop();
  CHECK(!audition.running());
}

TRENCH_TEST(audition_button_reports_device_state) {
  requireAudible();
  MainWindow window;
  window.show();
  QTest::qWait(50);
  auto* button = window.findChild<QPushButton*>(QStringLiteral("auditionSwitch"));
  auto* status = window.findChild<QLabel*>(QStringLiteral("status"));
  CHECK(button != nullptr);
  CHECK(status != nullptr);
  CHECK(button->isCheckable());
  CHECK(!button->isChecked());
  button->click();
  QTest::qWait(300);
  const QString text = status->text();
  std::printf("status after press: %s\n", text.toUtf8().constData());
  if (text.startsWith(QStringLiteral("AUDIO DEVICE"))) {
    CHECK(!button->isChecked());
    std::exit(kNoDeviceExit);
  }
  CHECK(text.startsWith(QStringLiteral("AUDITION OPEN")));
  CHECK(button->isChecked());
  button->click();
  QTest::qWait(100);
  CHECK(!button->isChecked());
  CHECK(status->text() == QStringLiteral("AUDITION CLOSED"));
  window.close();
}
