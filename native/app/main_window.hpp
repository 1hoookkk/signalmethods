#pragma once

#include <QMainWindow>
#include <QUndoStack>

#include <filesystem>

class ResponsePlotWidget;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(const std::filesystem::path& body_path,
                      double sample_rate_hz,
                      QWidget* parent = nullptr);

  [[nodiscard]] ResponsePlotWidget* responsePlot() const noexcept;

 private:
  ResponsePlotWidget* response_plot_{};
  QUndoStack undo_stack_;
};
