#pragma once

#include "editor_state.hpp"

#include <QString>

#include <optional>

namespace trench::app {

QString saveBody240(const EditorState& state, const QString& path);
QString saveDocument(const EditorState::Document& document, const QString& path);
std::optional<EditorState::Document> loadDocument(const QString& path,
                                                  QString* error);

}  // namespace trench::app
