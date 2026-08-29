#include "harness.hpp"

#include <QApplication>

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
