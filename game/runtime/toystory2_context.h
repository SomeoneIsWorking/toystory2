#pragma once

#include "audio/sound_bank.h"
#include "boot/graphics_sync.h"
#include "boot/guest_main_boot.h"
#include "boot/level_start_presentation.h"
#include "fmv/movie_player.h"
#include "fps60/camera_history.h"
#include "fps60/projection_scopes.h"
#include "input/pad_owner.h"
#include "overlay/overlay_images.h"
#include "render/scene_history.h"
#include "widescreen/resident_widescreen.h"

class Core;

namespace ts2 {

// Everything one Core owns, in the order a run reaches it. The runtime registers the registrations
// (`*::install`), the frame turn calls the operations, and the per-Core guest state each owner reads
// or advances lives in guest RAM rather than here.
struct ToyStory2Context {
  // Boot: the guest main prefix, the graphics and field-barrier replacements, and the level start's
  // first presentation.
  GuestMainBoot guestMainBoot;
  GraphicsSync graphicsSync;
  LevelStartPresentation levelStartPresentation;
  // Loading: the code images the guest's loader fills, the whole-file read, and the sound banks each
  // asset decode opens.
  OverlayImages overlays;
  audio::SoundBankProcessor soundBank;
  // Playback: the native pad, and the movie player that replaces the FMV overlay's own once the FMV
  // image is published.
  PadOwner pad;
  fmv::GuestMoviePlayer moviePlayer;
  // Presentation: the resident observation and provenance the 60 fps in-between pairs from, the
  // authored camera, and the 16:9 widening of the resident frame canvas.
  ResidentCameraHistory camera;
  ResidentSceneHistory scene;
  render::ResidentProjectionScopes projectionScopes;
  ResidentWidescreenProjection widescreen;
  // True while a guest call suspended between display fields (the front-end poll) is running. Its
  // field barrier then waits for the host: it completes with the fields it asked for and exits the
  // executor at the field boundary so the host can present, instead of spinning through the loop.
  bool yieldAtFieldBarrier = false;
};

ToyStory2Context &context(Core &core);

} // namespace ts2
