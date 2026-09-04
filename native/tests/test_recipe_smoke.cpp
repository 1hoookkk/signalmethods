#include "harness.hpp"

#include "cascade_plot.hpp"
#include "body_io.hpp"
#include "editor_state.hpp"
#include "ladder.hpp"
#include "main_window.hpp"
#include "morph_pad.hpp"
#include "number_box.hpp"
#include "rows_table.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QUrl>
#include <QPixmap>
#include <QAction>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QToolButton>

#include <QMenu>

#include <array>
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
  CHECK(window.findChild<NumberBox*>(QStringLiteral("noteLo0")) != nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("type0")) != nullptr);
  CHECK(window.findChild<QToolButton*>(QStringLiteral("fromAudio")) != nullptr);
  CHECK(window.findChild<QPushButton*>(QStringLiteral("frames")) != nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("keyframeGrid")) != nullptr);
  CHECK(window.findChild<QComboBox*>(QStringLiteral("fromAudioMode")) == nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("axes")) == nullptr);
  CHECK(window.findChild<QPushButton*>(QStringLiteral("anchorNow")) == nullptr);
  CHECK(window.findChild<QPushButton*>(QStringLiteral("anchorSquare")) == nullptr);
  auto* anchor = window.findChild<QToolButton*>(QStringLiteral("anchor"));
  CHECK(anchor != nullptr && anchor->isChecked());
  CHECK(anchor->menu() != nullptr);
  CHECK(anchor->menu()->findChild<QAction*>(QStringLiteral("anchorNow")) != nullptr);
  CHECK(anchor->menu()->findChild<QAction*>(QStringLiteral("anchorSquare")) != nullptr);
  auto* from_audio = window.findChild<QToolButton*>(QStringLiteral("fromAudio"));
  CHECK(from_audio->menu() != nullptr);
  CHECK(from_audio->menu()->findChild<QAction*>(QStringLiteral("fromAudioSixBells")) !=
        nullptr);
  CHECK(from_audio->menu()->findChild<QAction*>(QStringLiteral("fromAudioSpeech")) !=
        nullptr);
  CHECK(from_audio->text() == QStringLiteral("FROM AUDIO · SIX BELLS"));
  const std::array<const char*, 10> bar{"openBody", "resetDocument", "frames",
                                        "fromAudio", "anchor",       "auditionSwitch",
                                        "soloStage", "slot",         "saveBody",
                                        "exportBody240"};
  int last_x = -1;
  int first_y = -1;
  for (const char* name : bar) {
    auto* control = window.findChild<QWidget*>(QString::fromUtf8(name));
    CHECK(control != nullptr);
    CHECK(control->isVisible());
    const QPoint at = control->mapTo(&window, QPoint(0, 0));
    std::printf("bar %s x %d y %d\n", name, at.x(), at.y());
    CHECK(at.x() > last_x);
    last_x = at.x();
    if (first_y < 0) first_y = at.y();
    CHECK(std::abs(at.y() - first_y) <= 4);
  }
  CHECK(window.findChild<QComboBox*>(QStringLiteral("frame")) == nullptr);
  CHECK(window.findChild<QComboBox*>(QStringLiteral("template")) == nullptr);
  CHECK(window.findChild<QPushButton*>(QStringLiteral("corner0")) == nullptr);
  CHECK(window.findChild<QWidget*>(QStringLiteral("rowTable")) == nullptr);
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
  auto* rows = window.findChild<QWidget*>(QStringLiteral("rowsTable"));
  auto* pad = window.findChild<QWidget*>(QStringLiteral("morphPad"));
  CHECK(plot != nullptr);
  CHECK(rows != nullptr);
  CHECK(pad != nullptr);
  const QPoint plot_origin = plot->mapTo(&window, QPoint(0, 0));
  const QPoint rows_origin = rows->mapTo(&window, QPoint(0, 0));
  const QPoint pad_origin = pad->mapTo(&window, QPoint(0, 0));
  CHECK(rows_origin.y() > plot_origin.y() + plot->height());
  CHECK(pad_origin.y() >= plot_origin.y() + plot->height());
  CHECK(pad_origin.x() < rows_origin.x());
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

namespace {

QString barnFolder() {
  const char* configured = std::getenv("TRENCH_SMOKE_DIR");
  const QString folder = configured != nullptr && *configured != '\0'
                             ? QString::fromLocal8Bit(configured)
                             : QDir::tempPath() + QStringLiteral("/trench_smoke");
  QDir().mkpath(folder);
  return folder;
}

}

TRENCH_TEST(so_far_cells_add_up) {
  MainWindow window;
  window.resize(1560, 860);
  window.show();
  QTest::qWait(60);
  window.openPath(QString::fromUtf8(TRENCH_P2K_BODIES) +
                  QStringLiteral("/ooh_to_eee.body240"));
  EditorState& state = window.state();
  state.setPadPosition(0.3, 0.2);
  QTest::qWait(30);
  auto* rows = window.findChild<RowsTable*>(QStringLiteral("rowsTable"));
  CHECK(rows != nullptr);
  for (std::size_t row = 0; row < trench::core::native::kSections; ++row) {
    auto* cell = window.findChild<QWidget*>(QStringLiteral("soFar%1").arg(row));
    CHECK(cell != nullptr);
    CHECK(cell->width() == 72 && cell->height() == 22);
  }
  const std::array<double, 3> probes{611.0, 958.0, 4'936.0};
  const trench::core::Cascade at_pad = state.cascade(EditorState::kDatumHz);
  for (std::size_t through = 1; through <= trench::core::native::kSections; ++through) {
    for (const double hz : probes) {
      double want = 0.0;
      for (std::size_t index = 0; index < through; ++index) {
        want += trench::core::section_response_db(at_pad[index], hz,
                                                  EditorState::kDatumHz);
      }
      CHECK_NEAR(rows->soFarDbAt(through, hz), want, 0.01);
    }
  }
  std::printf("so far k3 958 Hz %+.2f dB, k6 958 Hz %+.2f dB\n",
              rows->soFarDbAt(3, 958.0), rows->soFarDbAt(6, 958.0));
  CHECK(window.plot() != nullptr);
  for (const double hz : probes) {
    CHECK_NEAR(rows->soFarDbAt(6, hz), window.plot()->curveDbAt(hz), 0.1);
  }
  for (std::size_t corner = 0; corner < EditorState::kCorners; ++corner) {
    if (state.sectionEnabledAt(corner, 3)) state.toggleSectionAt(corner, 3);
  }
  QTest::qWait(30);
  for (const double hz : probes) {
    CHECK_NEAR(rows->soFarDbAt(4, hz), rows->soFarDbAt(3, hz), 1.0e-9);
  }
  window.close();
}

TRENCH_TEST(pad_axis_names_rename_inline) {
  MainWindow window;
  window.resize(1560, 860);
  window.show();
  QTest::qWait(60);
  CHECK(window.findChild<QWidget*>(QStringLiteral("axes")) == nullptr);
  MorphPad* pad = window.pad();
  CHECK(pad != nullptr);

  QTest::mouseClick(pad, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(90, 158));
  auto* entry = pad->findChild<QLineEdit*>(QStringLiteral("axisEntry"));
  CHECK(entry != nullptr);
  CHECK(entry->isVisible());
  entry->setText(QStringLiteral("BRIGHT"));
  QTest::keyClick(entry, Qt::Key_Return);
  QTest::qWait(20);
  CHECK(!entry->isVisible());
  CHECK(pad->morphLabel() == QStringLiteral("BRIGHT"));

  QTest::mouseClick(pad, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(12, 80));
  CHECK(entry->isVisible());
  entry->setText(QStringLiteral("DROPPED"));
  QTest::keyClick(entry, Qt::Key_Escape);
  QTest::qWait(20);
  CHECK(!entry->isVisible());
  CHECK(pad->qLabel() == QStringLiteral("Q"));

  pad->renameMorphAxis(QStringLiteral("BRIGHT"));
  pad->renameQAxis(QStringLiteral("HARD"));
  CHECK(pad->morphLabel() == QStringLiteral("BRIGHT"));
  CHECK(pad->qLabel() == QStringLiteral("HARD"));

  EditorState& state = window.state();
  voiceLadder(state, 0);
  CHECK(state.axisNames().first == QStringLiteral("BRIGHT"));
  CHECK(state.axisNames().second == QStringLiteral("HARD"));
  const QString document = barnFolder() + QStringLiteral("/pad_axes.trenchbody");
  CHECK(trench::app::saveDocument(state.document(), document).isEmpty());
  QString error;
  const auto opened = trench::app::loadDocument(document, &error);
  CHECK(opened.has_value());
  CHECK(opened->morph_axis == QStringLiteral("BRIGHT"));
  CHECK(opened->q_axis == QStringLiteral("HARD"));
  window.openPath(document);
  CHECK(pad->morphLabel() == QStringLiteral("BRIGHT"));
  CHECK(pad->qLabel() == QStringLiteral("HARD"));
  window.close();
}
