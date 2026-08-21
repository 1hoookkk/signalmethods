#include "main_window.hpp"

#include "response_plot.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <QAction>
#include <QKeySequence>
#include <QUndoCommand>

#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) throw std::runtime_error("cannot open body: " + path.string());
  const auto size = static_cast<std::size_t>(stream.tellg());
  std::vector<std::uint8_t> bytes(size);
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  if (!stream) throw std::runtime_error("cannot read body: " + path.string());
  return bytes;
}

void write_section(trench::core::PackedBody& body, std::size_t section,
                   const trench::core::PackedSection& words) {
  body.words[0][section] = words;
  body.words[4][section] = words;
}

class SectionEditCommand final : public QUndoCommand {
 public:
  SectionEditCommand(MainWindow* window, std::size_t section,
                     trench::core::PackedSection before,
                     trench::core::PackedSection after)
      : window_(window), section_(section), before_(before), after_(after) {}

  void redo() override { window_->applySection(section_, after_); }
  void undo() override { window_->applySection(section_, before_); }

 private:
  MainWindow* window_;
  std::size_t section_;
  trench::core::PackedSection before_;
  trench::core::PackedSection after_;
};

}  // namespace

MainWindow::MainWindow(const std::filesystem::path& body_path,
                       double sample_rate_hz,
                       QWidget* parent)
    : QMainWindow(parent),
      body_(trench::core::PackedBody::from_body_bytes(read_bytes(body_path))),
      sample_rate_hz_(sample_rate_hz),
      freedom_mask_(trench::core::p2k::kAllFree),
      undo_stack_(this) {
  response_plot_ = new ResponsePlotWidget(this);
  response_plot_->setObjectName(QStringLiteral("responsePlot"));
  response_plot_->setBody(&body_, sample_rate_hz_, body_path.filename().string());
  response_plot_->setFreedomMask(freedom_mask_);
  setCentralWidget(response_plot_);
  setWindowTitle(QStringLiteral("TRENCH — %1").arg(QString::fromStdString(body_path.stem().string())));
  resize(960, 540);

  connect(response_plot_, &ResponsePlotWidget::gestureStarted, this,
          [this](std::size_t section) { before_words_ = body_.words[0][section]; });
  connect(response_plot_, &ResponsePlotWidget::sectionEdited, this,
          [this](std::size_t section, const trench::core::PackedSection& words) {
            write_section(body_, section, words);
            response_plot_->refresh();
          });
  connect(response_plot_, &ResponsePlotWidget::gestureFinished, this,
          [this](std::size_t section) {
            if (body_.words[0][section] == before_words_) return;
            undo_stack_.push(new SectionEditCommand(this, section, before_words_,
                                                    body_.words[0][section]));
          });
  connect(response_plot_, &ResponsePlotWidget::pinToggled, this,
          [this](std::size_t section, ResponsePlotWidget::Lane lane) {
            const auto bit = lane == ResponsePlotWidget::Lane::kPole
                                 ? trench::core::p2k::pole_bit(section)
                                 : trench::core::p2k::zero_bit(section);
            freedom_mask_ ^= bit;
            response_plot_->setFreedomMask(freedom_mask_);
          });

  auto* undo_action = undo_stack_.createUndoAction(this);
  undo_action->setShortcut(QKeySequence::Undo);
  addAction(undo_action);
  auto* redo_action = undo_stack_.createRedoAction(this);
  redo_action->setShortcut(QKeySequence::Redo);
  addAction(redo_action);
}

ResponsePlotWidget* MainWindow::responsePlot() const noexcept { return response_plot_; }

const trench::core::PackedBody& MainWindow::body() const noexcept { return body_; }

QUndoStack* MainWindow::undoStack() noexcept { return &undo_stack_; }

std::uint32_t MainWindow::freedomMask() const noexcept { return freedom_mask_; }

void MainWindow::applySection(std::size_t section,
                              const trench::core::PackedSection& words) {
  write_section(body_, section, words);
  response_plot_->refresh();
}
