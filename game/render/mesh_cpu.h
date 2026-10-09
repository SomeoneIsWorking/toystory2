// The registers and memory a resident mesh submitter's guest code runs on, so its subroutines and their
// calling conventions port one to one: a callee reads the caller's registers and leaves its own behind, and the
// scratchpad stack, the screen-space temporaries and the returned v1 all show those values.
#pragma once

#include "emit_memory.h"
#include "render/mesh_culling.h"
#include "render/mesh_scratch.h"

#include <cstdint>

namespace ts2 {

class MeshCpu : public CullRegisters {
public:
  explicit MeshCpu(const psx::present::EmitMemory &memory) : m_(memory) {}

  std::uint32_t a0 = 0, a1 = 0, a2 = 0;
  std::uint32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0, t8 = 0, t9 = 0;
  std::uint32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0;
  std::uint32_t ra = 0;
  static constexpr std::uint32_t s7 = kMeshScratch;

  // Set when a path without a native body is reached; the registers and memory are then meaningless.
  bool unported = false;

  const psx::present::EmitMemory &memory() const {
    return m_;
  }

  std::uint32_t lw(std::uint32_t a) const {
    return m_.mem_r32(a);
  }
  std::uint32_t lh(std::uint32_t a) const {
    return static_cast<std::uint32_t>(m_.mem_r16s(a));
  }
  std::uint32_t lhu(std::uint32_t a) const {
    return m_.mem_r16(a);
  }
  std::uint32_t lb(std::uint32_t a) const {
    return static_cast<std::uint32_t>(m_.mem_r8s(a));
  }
  std::uint32_t lbu(std::uint32_t a) const {
    return m_.mem_r8(a);
  }
  void sw(std::uint32_t a, std::uint32_t v) const {
    m_.mem_w32(a, v);
  }
  void sh(std::uint32_t a, std::uint32_t v) const {
    m_.mem_w16(a, static_cast<std::uint16_t>(v));
  }
  void sb(std::uint32_t a, std::uint32_t v) const {
    m_.mem_w8(a, static_cast<std::uint8_t>(v));
  }

  static std::uint32_t sra(std::uint32_t v, unsigned n) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> n);
  }
  static std::uint32_t slt(std::uint32_t a, std::uint32_t b) {
    return static_cast<std::int32_t>(a) < static_cast<std::int32_t>(b) ? 1u : 0u;
  }
  static std::int32_t sgn(std::uint32_t v) {
    return static_cast<std::int32_t>(v);
  }

  void gteOp(std::uint32_t command) const {
    gte_op(&m_.core(), command);
  }

private:
  const psx::present::EmitMemory &m_;
};

} // namespace ts2
