// resident_widescreen.h — the title-owned 16:9 widening of Toy Story 2's resident frame canvas.
#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

class Core;

namespace ts2 {

// The resident frame canvas SLUS_008.93 authors, read off a disc-backed run: GP1(05)/(07)/(08)
// give a 512-column frame of 240 rows, the guest alternates ONE 512-wide canvas between the two
// halves of its 1024x512 VRAM, and the graphics initializer 0x8003A650 publishes the projection once
// with literal arguments — SetGeomOffset(OFX=256, OFY=120) and SetGeomScreen(H=160).
//
// 512x240 is the 4:3 frame the console scans out, so 16:9 of the SAME rows at square pixels is 684
// columns. The widening is therefore a canvas width plus the horizontal projection centre that goes
// with it (342); the projection DISTANCE stays at retail 160, so objects keep their size and the
// picture is a wider view of the room rather than a magnified one.
//
// THREE SEAMS, each at a cited retail address:
//
//   1. The canvas, at the linked libgpu PutDrawEnv leaf 0x80086BD0. This title rasterizes
//      immediately, so a register re-assert after the guest's update is a frame too late.
//   2. The projection centre, published by 0x8003A650 as the literal OFX 256; the owner re-centres
//      it to the wide canvas per field and leaves OFY and the distance alone.
//   3. The per-object visibility window at 0x80027AF0, which the guest's visibility pass hands the
//      literal console frame (0, 0x200, 0, 0xf0) and then recurses through the object tree, passing
//      each parent's box as the child's window. Its flag is read only by the renderer's two
//      mesh-submission passes, so widening it draws more and decides nothing.
//   4. The screen rectangle the mesh submitter PUBLISHES at 0x80010000, which is where the widened
//      frame actually takes effect: the leaf above clamps each stored box to the console frame
//      (0x200 at 0x800280D4) before the submitter reads it, so the leaf's rectangle alone cannot
//      keep a margin object submitted. Measured, both directions.
//
// Every method is inert unless the 16:9 plan is active, so the 4:3 leg is a no-op.
struct ResidentFrameCanvas {
  int width = 512;
  int height = 240;
  int top = 256;     // the VRAM row the canvas starts at, measured from GP1(05)/GP0(E3)
  int centreY = 120; // the retail OFY, from SetGeomOffset at 0x8003A650
};

// THE WIDENING. One owner, per Core, held by the title context: it follows the guest's own display
// mode, corrects the guest-side publication of the canvas the guest is about to draw into, re-centres
// the horizontal projection per field, widens the guest's per-object visibility window, and presents
// the widened window. Every method is a no-op in the 4:3 leg, which is why 4:3 is byte-identical.
//
// WHY THE GUEST-SIDE SEAM AND NOT A REGISTER RE-ASSERT: the guest re-publishes its own drawing
// environment, drawing offset and display origin each field, alternating between the two 512-wide
// halves, and this title's rasterization is IMMEDIATE — the guest's clip reaches the framebuffer
// while its own code runs. A correction written to the GTE and GP0 registers after the update is
// therefore true for one frame and gone by the next, which is exactly what the previous attempt
// produced: a 684-wide canvas crossing into the half being displayed, sampled at the guest's
// 512-wide width, so the player saw a shifted crop with black slabs. The one place the correction
// can still take effect is the guest's own publication of the canvas, in the linked libgpu leaf.
class ResidentWidescreenProjection {
public:
  // Install the guest-side seams: the linked libgpu PutDrawEnv leaf, which is where the guest
  // publishes the clip, the clip extent and the drawing offset of the canvas it is about to draw
  // into, and the screen-rect publisher, which is where the guest states the rectangle its mesh
  // submitter draws against.
  void install(Core &core);

  // Follow the guest's own display mode, once per field. The widening's denominator is the width the
  // guest itself scans out, and the guest publishes GP1(08) some time AFTER its graphics initializer
  // returns: publishing inside the initializer measured a 256- or 320-wide front end and latched a
  // 428-column plan that a 512-column resident frame could never widen past.
  void syncToGuestDisplay(Core &core);

  // The horizontal projection centre, before the guest transforms this field's vertices, and the
  // clip, the drawing offset, the display origin and the backdrop cover after its update and before
  // the captured GP0 stream is rasterized.
  void beginField(Core &core) const;
  void presentField(Core &core) const;

  // The horizontal geometry of ONE guest DRAWENV, widened. Public because the guest-side seam is
  // the only place the correction can take effect (see the class comment).
  void widenDrawEnv(Core &core, std::uint32_t drawEnv);

  // The horizontal screen rectangle ONE guest object cull was handed, widened. Public for the same
  // reason: the rectangle is a register argument at the call site, and the cull is what decides
  // whether an object's mesh is submitted at all.
  void widenCullRect(Core &core) const;

  // The horizontal extent of the screen rectangle the guest is about to PUBLISH for the object it
  // is submitting, widened to this canvas. This is the seam the widened frame has to cross: the
  // visibility leaf clamps each object's stored screen box to the console frame before the mesh
  // submitter publishes it, so the leaf's own rectangle cannot keep a margin object alive and this
  // publisher is where the window is actually stated. See the measured note at
  // kScreenRectPublisherLeaf.
  void widenScreenRect(Core &core) const;

  // The left edge, in the guest's own screen space, that the widened cull must reach: the console
  // frame's left edge minus the framework's horizontal margin. The guest's projection centre moves
  // to the wide canvas centre, so the world that 4:3 culled now lands at screen x >= 0 and the
  // margin's worth of columns to its left is what the cull has to stop dropping.
  int cullLeft() const {
    return -horizontalMargin();
  }

  bool active() const {
    return active_;
  }
  const GuestProjectionPlan &plan() const {
    return plan_;
  }
  // The width of the canvas the guest is drawing into, 684 in the wide leg and the guest's own 512
  // otherwise. The guest's HUD and 2D elements are drawn against this, so an edge-anchored element
  // follows the widened edge on its own.
  int drawWidth() const {
    return active_ ? plan_.guestDrawWidth : canvas_.width;
  }
  // The horizontal margin the framework's own 2D widescreen layout adds to every prim the guest
  // submits: (wide width - native width) / 2. It is a property of the plan, not of this title, and
  // the canvas geometry below is expressed in the same terms.
  int horizontalMargin() const {
    return plan_.projectionHorizontalMargin;
  }
  // The VRAM column the wide picture starts at.
  int canvasOriginX() const {
    return canvasOriginX_;
  }

private:
  void publish(Core &core);
  void retire();
  ResidentFrameCanvas canvas_{};
  GuestProjectionPlan plan_{};
  bool active_ = false;
  int canvasOriginX_ = 0;
};

} // namespace ts2
