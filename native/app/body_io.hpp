#pragma once

#include "editor_state.hpp"
#include "import_routing.hpp"

#include <QString>

#include <optional>

namespace trench::app {

QString saveBody240(const EditorState& state, const QString& path);
void applyPeqList(EditorState& state, const PeqList& list);
QString saveDocument(const EditorState::Document& document, const QString& path);
std::optional<EditorState::Document> loadDocument(const QString& path,
                                                  QString* error);

}
