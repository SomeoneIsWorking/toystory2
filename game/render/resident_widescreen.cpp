#include "render/resident_widescreen.h"

#include "core.h"
#include "core/guest_execution.h"
#include "core/toystory2_context.h"
#include "game.h"
#include "gpu_native_internal.h" // gpu_gp0 — the same entry the guest's own GP0 stream uses
#include "gpu_vk.h"              // gpu_vk_latch_guest_projection
#include "proj_params.h"         // libgte_set_geom_offset

#include <cstdint>
#include <lucent/log.h>

namespace ts2 {
namespace {

// GP0(E3) draw-area top-left and GP0(E4) bottom-right: X in bits 0..9, Y in bits 10..18.
constexpr std::uint32_t kDrawAreaTopLeftOp = 0xE3000000u;
constexpr std::uint32_t kDrawAreaBottomRightOp = 0xE4000000u;
// GP0(E5) drawing offset: two adjacent SIGNED 11-bit axes, the second starting one axis-width up.
constexpr std::uint32_t kDrawOffsetOp = 0xE5000000u;
constexpr unsigned kVramAxisMask = 0x3FFu;
constexpr unsigned kRowAxisMask = 0x1FFu;
constexpr unsigned kDrawOffsetAxisShift = 11;

// GP1(05) display area start in VRAM, so the presented window is the canvas the guest just drew.
constexpr std::uint32_t kDisplayAreaStartOp = 0x05000000u;

// The linked libgpu leaf that publishes a DRAWENV, MEASURED on SLUS_008.93 and DECOMPILED WHOLE (Ghidra
// project under build/ghidra, tools/ghidra_wide_batch.py): 0x80086BD0 loads the structure's x/y at
// +0/+2 and emits GP0(E3), adds w-1 / h-1 from +4/+6 and emits GP0(E4), then emits GP0(E5) from the
// drawing offset at +8/+0xA. Its two callers, FUN_800860C8 and FUN_800861CC, pass $a1 as the DRAWENV
// and $a0 as the GPU command buffer (`move $s0,$a1` / `move $s1,$a0` at 0x80086BD8). The two wrappers
// at 0x8008686C and 0x800869B8 read the same layout, so correcting the structure reaches whichever of
// them the guest calls.
//
// The leaf's only horizontal limit, `lh` of the libgpu DISPLAY-MODE table (FUN_80085594, ResetGraph,
// fills DAT_8009eb0c/0x8009eb0e from the mode table at 0x8009EB88), is the GPU's own output size for
// the reset mode — 1024x512 in the mode this title resets to, read from the image — not a 512-column
// screen bound, so the widened clip is not clamped by it.
constexpr std::uint32_t kPutDrawEnvLeaf = 0x80086BD0u;

// The guest's PER-OBJECT VISIBILITY CULL, DECOMPILED WHOLE (Ghidra, build/ghidra/ts2le). It is
// `FUN_80027af0(object, left, right, top, bottom, flag)`: it transforms the object's bounding box
// through the GTE and keeps the object only if the box is not entirely outside the rectangle it was
// handed. Its caller FUN_80029614 — the per-frame visibility pass, itself called from the renderer
// FUN_8002A070 — hands it the LITERAL console frame at 0x80029AAC:
//
//     FUN_80027af0(&DAT_800c0eb0 + DAT_800a1270 * 0x20, 0, 0x200, 0, 0xf0, 0xfe);
//                                                    ^  ^^^^^  ^  ^^^^
//                                                    |  512  |  240
//                                                    left
//
// and seeds every object's screen bounding box with the same (0,0)-(0x200,0xf0). FUN_80027af0 then
// RECURSES into child objects passing the parent's accumulated box as the child's rectangle, so the
// root rectangle is the whole tree's cull window and widening it here widens every level.
//
// GAMEPLAY READS: none. The visibility flag this pass writes (DAT_800a8860, one 0x34-byte record per
// object) is read by exactly two functions, FUN_80020074 and FUN_8002044C, and both are the renderer's
// own two mesh-submission passes — each skips an object's mesh records when the flag is 0 and
// otherwise transforms and emits them. The bounding box is read by the same two and by the sort. So
// this window decides what is DRAWN, not what HAPPENS, and widening it is a rendering change only.
constexpr std::uint32_t kObjectCullLeaf = 0x80027AF0u;

// THE GUEST'S SCREEN-RECT PUBLISHER, DECOMPILED WHOLE (Ghidra, exact bytes, 10 instructions at
// 0x80010000): `SetScreenRect(left, right, top, bottom)` shifts each of $a0..$a3 left by 16 and
// stores it at 0x1F800060/0x64/0x68/0x6C. Its reader is the twin at 0x8001002C. Four call sites
// publish into it (Ghidra xrefs): the renderer 0x8002A070 twice, the mesh submitter 0x8002622C
// (0x8002638C) and the second submitter 0x80026D34 (0x80026E84).
//
// WHY THIS IS THE SEAM THE WIDENED FRAME HAS TO CROSS. The visibility leaf 0x80027AF0 stores, per
// object, a SCREEN BOX in the record at 0x800A8864, and the mesh submitter publishes THAT BOX as
// the screen rect it submits the object's mesh records against. The leaf's own rectangle argument
// therefore never reaches the floor: the box it stores is the intersection of the projected box with
// a rect whose corners the leaf has already CLAMPED to the console frame — the literal `slti
// $v0,$v0,0x200` at 0x800280D4 and the `sh $s6,($s0)` stores of $s6 = 0x200 at 0x800280F8. So an
// object whose box lies past the console's right edge is stored as a sliver ending AT column 512,
// whatever rectangle the caller passed, and the submitter then drops every mesh record outside it.
// MEASURED, both ways: widening the leaf's rectangle alone to (-4096, 4096) changed the frame by
// ZERO pixels, and publishing the canvas rectangle here filled every one of the missing floorboards.
constexpr std::uint32_t kScreenRectPublisherLeaf = 0x80010000u;

// A guest DRAWENV, exactly as the leaf above reads it.
struct DrawEnvFields {
  std::uint32_t x = 0x00u;
  std::uint32_t y = 0x02u;
  std::uint32_t width = 0x04u;
  std::uint32_t height = 0x06u;
  std::uint32_t offsetX = 0x08u;
  std::uint32_t offsetY = 0x0Au;
};

// The guest's display-origin word, MEASURED at the buffer swap 0x8003A3A0 (`lw $v1,0x4EC($gp)`,
// stored into both buffers' display environments at +0x278). It is deliberately NOT written: it is
// guest state the game positions its own layers against, and the widening does not need to move it —
// the presented window is set at the display register instead (presentField). Measured cost of
// writing it: the level-intro title card, which the 4:3 leg does not show at that frame, appeared in
// the left margin, because the card is positioned against this word.

std::uint32_t drawAreaTopLeft(int x, int y) {
  return kDrawAreaTopLeftOp | (static_cast<std::uint32_t>(x) & kVramAxisMask) |
         ((static_cast<std::uint32_t>(y) & kRowAxisMask) << 10);
}

std::uint32_t drawAreaBottomRight(int x, int y) {
  return kDrawAreaBottomRightOp | (static_cast<std::uint32_t>(x) & kVramAxisMask) |
         ((static_cast<std::uint32_t>(y) & kRowAxisMask) << 10);
}

std::uint32_t drawOffset(int x, int y) {
  return kDrawOffsetOp | (static_cast<std::uint32_t>(x) & 0x7FFu) |
         ((static_cast<std::uint32_t>(y) & 0x7FFu) << kDrawOffsetAxisShift);
}

std::uint32_t displayAreaStart(int x, int y) {
  return kDisplayAreaStartOp | (static_cast<std::uint32_t>(x) & kVramAxisMask) |
         ((static_cast<std::uint32_t>(y) & kRowAxisMask) << 10);
}

void putDrawEnvOverride(Core *core) {
  ResidentWidescreenProjection &widescreen = context(*core).widescreen;
  if (widescreen.active()) {
    // MEASURED argument order of the leaf: `move $s0,$a1` / `move $s1,$a0` at 0x80086BD8, and every
    // field it publishes is loaded through $s0 — so the DRAWENV it reads is $a1, not $a0.
    widescreen.widenDrawEnv(*core, core->r[5]);
  }
  callOriginalToReturn(*core, kPutDrawEnvLeaf, "resident draw environment original");
}

void objectCullOverride(Core *core) {
  context(*core).widescreen.widenCullRect(*core);
  callOriginalToReturn(*core, kObjectCullLeaf, "object visibility cull original");
}

void screenRectPublisherOverride(Core *core) {
  context(*core).widescreen.widenScreenRect(*core);
  callOriginalToReturn(*core, kScreenRectPublisherLeaf, "screen rect publisher original");
}

} // namespace

void ResidentWidescreenProjection::install(Core &core) {
  installResidentOverride(core, kPutDrawEnvLeaf, "resident-draw-env", putDrawEnvOverride);
  installResidentOverride(core, kObjectCullLeaf, "object-visibility-cull", objectCullOverride);
  installResidentOverride(core, kScreenRectPublisherLeaf, "screen-rect-publisher", screenRectPublisherOverride);
}

void ResidentWidescreenProjection::syncToGuestDisplay(Core &core) {
  const int guestWidth = core.game->gpu.s_disp_w;
  if (guestWidth == canvas_.width) {
    if (!active_) {
      publish(core);
    }
  } else if (active_) {
    retire();
  }
}

void ResidentWidescreenProjection::publish(Core &core) {
  // The framework's own guest-projection plan for this canvas: the draw width the guest is to fill
  // at 16:9, the native width it scanned out at 4:3, and the display width the front end presents.
  // Only the canvas width is used here; the guest's projection registers are left at retail (see the
  // class comment), so this is a plan, not a projection change.
  plan_ = gpu_vk_latch_guest_projection(&core,
                                        {
                                            .extent = {canvas_.width, canvas_.height},
                                            .drawWidth = canvas_.width,
                                        });
  active_ = plan_.widescreen();
  if (!active_) {
    return;
  }
  // ONE canvas. The guest alternates a 512-wide canvas between the two halves of its VRAM; a
  // 684-wide canvas cannot do that without the two copies overlapping, so the wide leg owns a single
  // canvas and re-asserts its clip, drawing offset and display origin every field.
  //
  // It sits one framework margin in (86 columns), because ws_2d_local_x in gpu_native.cpp offsets
  // everything the guest submits by (wide - native) / 2. So the clip and the display window both
  // start there, and the presented window is exactly the one the guest drew.
  canvasOriginX_ = plan_.projectionHorizontalMargin;
}

void ResidentWidescreenProjection::retire() {
  active_ = false;
}

void ResidentWidescreenProjection::widenDrawEnv(Core &core, std::uint32_t drawEnv) {
  // Only the resident frame band is widened. The guest's SECOND draw environment is the upper VRAM
  // band (y=0, the level's texture pages) and must keep the geometry retail authored for it, so the
  // band is identified by its measured top row and height rather than by "whatever is published".
  if (core.mem_r16(drawEnv + DrawEnvFields{}.y) != static_cast<std::uint16_t>(canvas_.top) ||
      core.mem_r16(drawEnv + DrawEnvFields{}.height) != static_cast<std::uint16_t>(canvas_.height)) {
    return;
  }
  // The clip and the drawing offset move with the framework's margin, because the guest's canvas
  // coordinate c is rasterized at VRAM c + margin and the presented window is the drawn one.
  const DrawEnvFields fields{};
  core.mem_w16(drawEnv + fields.x, 0);
  core.mem_w16(drawEnv + fields.y, static_cast<std::uint16_t>(canvas_.top));
  core.mem_w16(drawEnv + fields.width, static_cast<std::uint16_t>(drawWidth()));
  core.mem_w16(drawEnv + fields.height, static_cast<std::uint16_t>(canvas_.height));
  core.mem_w16(drawEnv + fields.offsetX, 0);
  core.mem_w16(drawEnv + fields.offsetY, static_cast<std::uint16_t>(canvas_.top));
}

void ResidentWidescreenProjection::widenCullRect(Core &core) const {
  if (!active_) {
    return;
  }
  // $a1 = the rectangle's left edge, $a2 = its right edge (see kObjectCullLeaf). Widening is
  // MONOTONE: the rectangle only ever grows, so no object that 4:3 drew stops being drawn. The
  // vertical edges ($a3, $a4) and the mode flag ($a5) are left exactly as the guest passed them.
  core.r[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(cullLeft()));
  core.r[6] = static_cast<std::uint32_t>(drawWidth());
}

void ResidentWidescreenProjection::widenScreenRect(Core &core) const {
  if (!active_) {
    return;
  }
  // $a0 = left, $a1 = right, $a2 = top, $a3 = bottom — the rectangle the guest is about to publish
  // for the object it is submitting (see kScreenRectPublisherLeaf). Its HORIZONTAL extent becomes
  // the canvas the guest is drawing into, which is what the widened projection means: the left
  // edge is the canvas's own edge at 0 and the right edge is its far column. The vertical extent
  // is the guest's, untouched, because the canvas is exactly as tall as the console frame. This is
  // a SUBMISSION WINDOW — it decides what is DRAWN and nothing else — and it only ever grows, so
  // in the wide leg no object 4:3 submitted is dropped.
  core.r[4] = 0;
  core.r[5] = static_cast<std::uint32_t>(drawWidth());
}

void ResidentWidescreenProjection::beginField(Core &core) const {
  if (!active_) {
    return;
  }
  // The horizontal projection centre only. OFY and the projection DISTANCE stay at the values the
  // guest's own graphics initializer published (120 and 160, Ghidra-decompiled from 0x8003A650), so
  // the vertical projection and every object's size and distance from the frame centre are retail's.
  libgte_set_geom_offset(&core, plan_.projectionCenterX, canvas_.centreY);
  core.rsub.projParams.setGeomOfxForAspect(static_cast<float>(plan_.projectionCenterX));
}

void ResidentWidescreenProjection::presentField(Core &core) const {
  if (!active_) {
    return;
  }
  const int right = canvasOriginX_ + drawWidth() - 1;
  const int bottom = canvas_.top + canvas_.height - 1;
  gpu_gp0(&core, drawAreaTopLeft(canvasOriginX_, canvas_.top));
  gpu_gp0(&core, drawAreaBottomRight(right, bottom));
  gpu_gp0(&core, drawOffset(canvasOriginX_, canvas_.top));
  gpu_gp1(&core, displayAreaStart(canvasOriginX_, canvas_.top));
}

} // namespace ts2
