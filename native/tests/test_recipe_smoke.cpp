#include "harness.hpp"

#include "cascade_plot.hpp"
#include "body_io.hpp"
#include "editor_state.hpp"
#include "ladder.hpp"
#include "main_window.hpp"
#include "morph_pad.hpp"
#include "word_dial.hpp"

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QUrl>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <variant>

TRENCH_TEST(recipe_chrome_is_on_the_window) {
  MainWindow window;
  window.resize(1560, 860);
  window.show();
  QTest::qWait(60);
  for (const char* name : {"copyAcross", "soloStage", "exportBody240"}) {
    CHECK(window.findChild<QPushButton*>(QString::fromUtf8(name)) != nullptr);
  }
  CHECK(window.findChild<QWidget*>(QStringLiteral("zplaneView")) == nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("fitRoom")) == nullptr);
  CHECK(window.findChild<WordDial*>(QStringLiteral("freqDial0")) != nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("shape0")) != nullptr);
  CHECK(window.findChild<QPushButton*>(QStringLiteral("corner0")) != nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("cornerLevel")) == nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("armadilloEditor")) == nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("sectionStrip")) == nullptr);
  const char* configured = std::getenv("TRENCH_SMOKE_DIR");
  const QString folder = configured != nullptr && *configured != '\0'
                             ? QString::fromLocal8Bit(configured)
                             : QDir::tempPath() + QStringLiteral("/trench_smoke");
  QDir().mkpath(folder);
  QTest::qWait(60);
  auto* plot = window.findChild<QWidget*>(QStringLiteral("cascadePlot"));
  auto* rows = window.findChild<QWidget*>(QStringLiteral("rowTable"));
  auto* pad = window.findChild<QWidget*>(QStringLiteral("morphPad"));
  CHECK(plot != nullptr);
  CHECK(rows != nullptr);
  CHECK(pad != nullptr);
  CHECK(rows->geometry().top() > plot->geometry().bottom());
  CHECK(pad->geometry().top() > plot->geometry().bottom());
  CHECK(pad->geometry().left() > rows->geometry().right());
  std::printf("chrome sizeHint %dx%d\n", window.sizeHint().width(),
              window.sizeHint().height());
  std::printf("console sizeHint %dx%d\n", rows->sizeHint().width(),
              rows->sizeHint().height());
  std::printf("row table %dx%d\n", rows->width(), rows->height());
  const QPixmap frame = window.grab();
  const QString path = folder + QStringLiteral("/recipe_chrome.png");
  CHECK(frame.save(path));
  std::printf("%s %dx%d\n", path.toUtf8().constData(), frame.width(), frame.height());
  window.close();
}

TRENCH_TEST(dropped_file_opens_like_the_open_button) {
  MainWindow window;
  window.resize(1560, 860);
  window.show();
  QTest::qWait(60);
  const char* configured = std::getenv("TRENCH_SMOKE_DIR");
  const QString folder = configured != nullptr && *configured != '\0'
                             ? QString::fromLocal8Bit(configured)
                             : QDir::tempPath() + QStringLiteral("/trench_smoke");
  QDir().mkpath(folder);
  const QString path = folder + QStringLiteral("/dropped.body240");
  {
    EditorState voiced;
    voiceLadder(voiced, 0);
    CHECK(trench::app::saveBody240(voiced, path).isEmpty());
  }
  QMimeData mime;
  mime.setUrls({QUrl::fromLocalFile(path)});
  QDragEnterEvent enter(QPoint(200, 300), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&window, &enter);
  CHECK(enter.isAccepted());
  QDropEvent drop(QPointF(200, 300), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&window, &drop);
  QTest::qWait(60);
  bool reported = false;
  for (QLabel* label : window.findChildren<QLabel*>()) {
    if (label->text().contains(QStringLiteral("BODY"))) reported = true;
  }
  CHECK(reported);
  const QPixmap frame = window.grab();
  const QString shot = folder + QStringLiteral("/dropped_body.png");
  CHECK(frame.save(shot));
  std::printf("%s\n", shot.toUtf8().constData());
  window.close();
}
