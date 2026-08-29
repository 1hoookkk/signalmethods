#include "harness.hpp"

#include "body_io.hpp"
#include "editor_state.hpp"
#include "template_shelf.hpp"

#include <QApplication>
#include <QDir>
#include <QString>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <set>
#include <string>

namespace trench::test {

std::vector<Case>& registry() {
  static std::vector<Case> cases;
  return cases;
}

}  // namespace trench::test

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  auto& cases = trench::test::registry();
  std::sort(cases.begin(), cases.end(), [](const auto& a, const auto& b) {
    return std::string(a.name) < std::string(b.name);
  });
  if (argc < 2) {
    std::fprintf(stderr, "usage: trench_native_tests <case> | --list | --registry <case>...\n");
    return 2;
  }
  const std::string command = argv[1];
  if (command == "--list") {
    for (const auto& item : cases) std::printf("%s\n", item.name);
    return 0;
  }
  if (command == "--bake-shelf") {
    if (argc < 3) {
      std::fprintf(stderr, "usage: trench_native_tests --bake-shelf <directory>\n");
      return 2;
    }
    const QString folder = QString::fromLocal8Bit(argv[2]);
    const bool with_zeros = argc > 3 && std::string(argv[3]) == "--zeros";
    QDir().mkpath(folder);
    int written = 0;
    for (const auto& entry : trench::app::kTemplateShelf) {
      EditorState state;
      state.loadTemplate(entry);
      if (with_zeros) state.applyZeroHabits();
      auto document = state.document();
      for (std::size_t corner = 1; corner < trench::core::native::kCorners; ++corner) {
        document.corners[corner] = document.corners[0];
      }
      state.setDocument(document);
      QString stem;
      for (const QChar letter : QString::fromUtf8(entry.name).toLower()) {
        stem.append(letter.isLetterOrNumber() ? letter : QChar('_'));
      }
      const QString path = folder + QStringLiteral("/") + stem + QStringLiteral(".body240");
      const QString refusal = trench::app::saveBody240(state, path);
      std::size_t poles = 0;
      for (const auto& pole : entry.poles) poles += pole.present ? 1 : 0;
      std::printf("%-22s %zu poles  %s\n", entry.name, poles,
                  refusal.isEmpty() ? path.toUtf8().constData() : refusal.toUtf8().constData());
      written += refusal.isEmpty() ? 1 : 0;
    }
    std::printf("%d bodies written\n", written);
    return 0;
  }
  if (command == "--registry") {
    std::set<std::string> listed;
    for (int i = 2; i < argc; ++i) listed.insert(argv[i]);
    int mismatches = 0;
    for (const auto& item : cases) {
      if (listed.erase(item.name) == 0) {
        std::printf("NOT REGISTERED WITH CTEST: %s\n", item.name);
        ++mismatches;
      }
    }
    for (const auto& name : listed) {
      std::printf("CTEST NAMES A MISSING CASE: %s\n", name.c_str());
      ++mismatches;
    }
    std::printf("%zu cases, %d mismatches\n", cases.size(), mismatches);
    return mismatches == 0 ? 0 : 1;
  }
  const auto found = std::find_if(cases.begin(), cases.end(), [&](const auto& item) {
    return command == item.name;
  });
  if (found == cases.end()) {
    std::fprintf(stderr, "unknown case: %s\n", command.c_str());
    return 2;
  }
  try {
    found->run();
  } catch (const trench::test::Failure& failure) {
    std::printf("FAIL %s\n%s\n", found->name, failure.what());
    return 1;
  } catch (const std::exception& error) {
    std::printf("ERROR %s\n%s\n", found->name, error.what());
    return 1;
  }
  std::printf("PASS %s\n", found->name);
  return 0;
}
