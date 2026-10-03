// resident_widescreen.h — the title-owned 16:9 widening of Toy Story 2's resident frame canvas.
#pragma once

#include "guest_widescreen_projection.h"

#include <cstdint>

class Core;

namespace ts2 {

// The resident frame canvas SLUS_008.93 authors: GP1(05)/(07)/(08) give a 512-column frame of 240
// rows, the guest alternates ONE 512-wide canvas between the two halves of its 1024x512 VRAM, and
// the graphics initializer 0x8003A650 publishes the projection once — SetGeomOffset(OFX=256, OFY=120)
// and SetGeomScreen(H=160). 16:9 of the same rows at square pixels is 684 columns, so the widening is
// a canvas width plus the horizontal projection centre that goes with it (342); the projection
// distance stays at retail 160, so objects keep their size.
//
// FOUR SEAMS, each at a cited retail address (see resident_widescreen.cpp):
//
//   1. The canvas, at the linked libgpu PutDrawEnv leaf 0x80086BD0 — the guest's own publication of
//      the canvas it is about to draw into, which is the only point where a correction still holds
//      because this title's rasterization is immediate.
//   2. The projection centre, published once by 0x8003A650 as the literal OFX 256 and re-centred per
//      field; OFY and the distance are left alone.
//   3. The per-object visibility window at 0x80027AF0, whose console frame is baked into the leaf and
//      unreachable through its arguments.
//   4. The screen rectangle the mesh submitter publishes at 0x80010000, which is where the widened
//      frame actually takes effect: the leaf above clamps each stored box to the console frame first.
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
// the widened window. The correction belongs at the guest's own publication of the canvas, not in a
// register re-assert after the update: the guest re-publishes its drawing environment, drawing offset
// and display origin every field, and this title's rasterization is immediate, so a register write
// after the update is true for one field and gone by the next.
class ResidentWidescreenProjection {
public:
  // Install the guest-side seams: the linked libgpu PutDrawEnv leaf, which is where the guest
  // publishes the clip, the clip extent and the drawing offset of the canvas it is about to draw
  // into, and the screen-rect publisher, which is where the guest states the rectangle its mesh
  // submitter draws against.
  void install(Core &core);

  // Follow the guest's own display mode, once per field, for the leg the host is actually
  // presenting. The widening's denominator is the width the guest itself scans out, and the guest
  // publishes GP1(08) some time AFTER its graphics initializer returns, so this runs per field.
  //
  // `residentFrame` is the half the guest cannot answer. The front end publishes 512-wide screens of
  // its own, so a width test alone widened a 2D menu; this widening is the RESIDENT frame's — the room
  // is 3D and its projection is what has to change — so the host says which leg it is presenting and
  // every front-end screen is presented at the width it authored, centred by the letterbox.
  void syncToGuestDisplay(Core &core, bool residentFrame);

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
  // whether an object's mesh is submitted at all. The widened leg hands the leaf the whole signed
  // range its screen boxes are stored in, not the canvas: the console frame is baked into the leaf
  // and a conservative box that reaches past the canvas would otherwise drop faces that are on it.
  void widenCullRect(Core &core) const;

  // The horizontal extent of the screen rectangle the guest is about to PUBLISH for the object it
  // is submitting, widened to this canvas. This is the seam the widened frame has to cross: the
  // visibility leaf clamps each object's stored screen box to the console frame before the mesh
  // submitter publishes it, so the leaf's own rectangle cannot keep a margin object alive. See the
  // measured note at kScreenRectPublisherLeaf.
  void widenScreenRect(Core &core) const;

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
  void retire(Core &core);
  ResidentFrameCanvas canvas_{};
  GuestProjectionPlan plan_{};
  bool active_ = false;
  int canvasOriginX_ = 0;
};

} // namespace ts2
