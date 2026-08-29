#include "harness.hpp"

#include "armadillo_editor.hpp"
#include "main_window.hpp"

#include <QComboBox>
#include <QDir>
#include <QImage>
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
  for (QWidget* child : window.findChildren<QWidget*>()) {
    const bool chrome = qobject_cast<QPushButton*>(child) != nullptr ||
                        qobject_cast<QComboBox*>(child) != nullptr ||
                        child->objectName() == QStringLiteral("referenceName");
    if (!chrome || child->minimumSizeHint().width() <= 0) continue;
    std::printf("  %-18s %-16s min %4d px\n",
                child->objectName().toUtf8().constData(),
                child->property("text").toString().toUtf8().constData(),
                child->minimumSizeHint().width());
  }
  window.resize(least);
  QTest::qWait(60);
  capture(window, QStringLiteral("min_armadillo.png"));
  CHECK(plane->height() == 230);
  auto* editor = static_cast<ArmadilloEditor*>(plane);
  CHECK(editor->discCentre().y() - editor->discRadius() >= 20.0);
  CHECK(editor->discCentre().y() + editor->discRadius() <= editor->height() - 2.0);
  CHECK(plot->height() >= 160);
  projection->click();
  capture(window, QStringLiteral("min_zplane.png"));

  window.resize(1060, 940);
  QTest::qWait(60);
  capture(window, QStringLiteral("normal_zplane.png"));
  projection->click();
  capture(window, QStringLiteral("normal_armadillo.png"));
  std::printf("plot %d px tall, plane %d px tall at 1060x940\n", plot->height(), plane->height());
  CHECK(plane->height() == 230);
  CHECK(plot->height() > plane->height());
  auto* strip = window.findChild<QWidget*>(QStringLiteral("sectionStrip"));
  CHECK(strip != nullptr);
  std::printf("strip %dx%d\n", strip->width(), strip->height());
  CHECK(strip->height() >= 100 && strip->height() <= 120);
  CHECK(strip->height() == strip->heightForWidth(strip->width()));
  const QImage frame = window.grab().toImage();
  const QPoint first_card = strip->mapTo(&window, QPoint(strip->width() / 2 - 2 * strip->height(), strip->height() / 2));
  const QColor painted = frame.pixelColor(first_card);
  std::printf("first card pixel %d,%d,%d\n", painted.red(), painted.green(), painted.blue());
  CHECK(painted.red() >= 240 && painted.green() >= 240 && painted.blue() >= 240);
  const QPoint plot_middle =
      plot->mapTo(&window, QPoint(plot->width() / 2, plot->height() / 2));
  const QColor interior = frame.pixelColor(plot_middle);
  std::printf("plot interior pixel %d,%d,%d\n", interior.red(), interior.green(),
              interior.blue());
  CHECK(interior.red() == 255 && interior.green() == 255 &&
        interior.blue() == 255);
  window.close();
}
