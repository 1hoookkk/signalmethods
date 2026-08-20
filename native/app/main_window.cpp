#include "main_window.hpp"

#include "response_plot.hpp"
#include "trench/core/packed_body.hpp"

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

}  // namespace

MainWindow::MainWindow(const std::filesystem::path& body_path,
                       double sample_rate_hz,
                       QWidget* parent)
    : QMainWindow(parent), undo_stack_(this) {
  const auto body = trench::core::PackedBody::from_body_bytes(read_bytes(body_path));
  response_plot_ = new ResponsePlotWidget(this);
  response_plot_->setObjectName(QStringLiteral("responsePlot"));
  response_plot_->setBody(body, sample_rate_hz, body_path.filename().string());
  setCentralWidget(response_plot_);
  setWindowTitle(QStringLiteral("TRENCH — %1").arg(QString::fromStdString(body_path.stem().string())));
  resize(960, 540);
}

ResponsePlotWidget* MainWindow::responsePlot() const noexcept { return response_plot_; }
