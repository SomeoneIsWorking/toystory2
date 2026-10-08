#include "boot/title_session.h"

#include <iostream>
#include <string_view>

int main(int argc, char **argv) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "-h" || argument == "--help") {
      std::cout << "usage: " << argv[0] << " [PS-X EXE]\n";
      return 0;
    }
  }

  const char *path = argc > 1 ? argv[1] : ts2::kDefaultExe;
  ts2::TitleSession session(path);
  return session.run();
}