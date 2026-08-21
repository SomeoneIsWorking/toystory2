#include "core.h"
#include "game.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <system_error>

using OverrideFn = void (*)(Core *);

void load_exe(const char *path, Core *core);
void main_dispatch(Core *core, std::uint32_t address);
int rec_func_index(std::uint32_t address);
void shard_set_override(std::uint32_t address, OverrideFn function);

namespace {

constexpr std::array<std::string_view, 32> kRegisterNames = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0",   "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};

std::uint32_t parse_address(std::string_view text, const char *label) {
  if (text.starts_with("0x") || text.starts_with("0X")) {
    text.remove_prefix(2);
  }
  std::uint32_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  if (error != std::errc{} || end != text.data() + text.size()) {
    std::fprintf(stderr, "REFUSED: %s is not a hexadecimal guest address\n", label);
    std::exit(2);
  }
  return value;
}

void capture_boundary(Core *core) {
  std::printf("# PORT-CALL-BOUNDARY-REGS pc=0x%08X\n", core->pc);
  for (std::size_t index = 1; index < kRegisterNames.size(); ++index) {
    std::printf("# PORT-CALL-BOUNDARY-REG %.*s=0x%08X\n",
                static_cast<int>(kRegisterNames[index].size()),
                kRegisterNames[index].data(),
                core->r[index]);
  }
  std::printf("# PORT-CALL-BOUNDARY-REG lo=0x%08X\n", core->lo);
  std::printf("# PORT-CALL-BOUNDARY-REG hi=0x%08X\n", core->hi);
  std::fflush(stdout);
  std::exit(EXIT_SUCCESS);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 4) {
    std::fprintf(stderr, "usage: %s <PS-X EXE> <entry> <first-call-target>\n", argv[0]);
    return 2;
  }
  const std::uint32_t entry = parse_address(argv[2], "entry");
  const std::uint32_t boundary = parse_address(argv[3], "first-call-target");
  if (rec_func_index(entry) < 0 || rec_func_index(boundary) < 0) {
    std::fprintf(stderr, "REFUSED: substrate omits entry 0x%08X or boundary 0x%08X\n", entry, boundary);
    return 2;
  }
  auto game = std::make_unique<Game>();
  Core *core = &game->core;
  load_exe(argv[1], core);
  shard_set_override(boundary, capture_boundary);
  main_dispatch(core, entry);
  std::fprintf(stderr, "FAIL: entry 0x%08X returned before boundary 0x%08X\n", entry, boundary);
  return 1;
}
