#include "harness.hpp"

#include "main_window.hpp"

#include <QDir>
#include <QPixmap>
#include <QPushButton>
#include <QTest>

#include <cstdio>
#include <cstdlib>

namespace {

QString smokeFolder() {
  const char* configured = std::getenv("TRENCH_SMOKE_DIR");
  const QString folder = configured != nullptr && *configured != '\0'
                             ? QString::fromLocal8Bit(configured)
                             : QDir::tempPath() + QStringLiteral("/trench_smoke");
  QDir().mkpath(folder);
  return folder;
}

void capture(MainWindow& window, const QString& name) {
  QTest::qWait(60);
  const QPixmap frame = window.grab();
  const QString path = smokeFolder() + QStringLiteral("/") + name;
  CHECK(frame.save(path));
  std::printf("%s %dx%d\n", path.toUtf8().constData(), frame.width(), frame.height());
}

}  // namespace

TRENCH_TEST(window_smoke_at_minimum_and_normal_size) {
  MainWindow window;
  window.show();
  QTest::qWait(60);
  auto* plot = window.findChild<QWidget*>(QStringLiteral("cascadePlot"));
  auto* plane = window.findChild<QWidget*>(QStringLiteral("armadilloEditor"));
  auto* projection = window.findChild<QPushButton*>(QStringLiteral("projectionSwitch"));
  CHECK(plot != nullptr);
  CHECK(plane != nullptr);
  CHECK(projection != nullptr);

  const QSize least = window.minimumSizeHint();
  std::printf("minimum size hint %dx%d\n", least.width(), least.height());
  window.resize(least);
  QTest::qWait(60);
  capture(window, QStringLiteral("min_armadillo.png"));
  CHECK(plane->height() == 230);
  CHECK(plot->height() >= 160);
  projection->click();
  capture(window, QStringLiteral("min_zplane.png"));

  window.resize(1180, 860);
  QTest::qWait(60);
  capture(window, QStringLiteral("normal_zplane.png"));
  projection->click();
  capture(window, QStringLiteral("normal_armadillo.png"));
  std::printf("plot %d px tall, plane %d px tall at 1180x860\n", plot->height(), plane->height());
  CHECK(plane->height() == 230);
  window.close();
}
