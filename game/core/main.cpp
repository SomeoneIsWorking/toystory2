// main.cpp — the Toy Story 2 port's process entry point.
//
// Parses arguments, constructs one TitleSession, and returns its exit code. The boot sequence
// (exe self-provision, load_exe, peripheral init, override registration, native boot) and the
// teardown belong to `ts2::TitleSession`; this file names no subsystem.
//
// Guest code is consumed directly from the authenticated executable and translated by psxport's
// dynarec. Native title owners remain explicit runtime overrides.
#include "title_session.h"

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