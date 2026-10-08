#pragma once

#include "audio/sound_bank.h"
#include "boot/graphics_sync.h"
#include "boot/guest_main_boot.h"
#include "boot/level_start_presentation.h"
#include "fmv/movie_player.h"
#include "frame/frame_cut.h"
#include "input/pad_owner.h"
#include "overlay/overlay_images.h"
#include "render/actor_incarnation.h"
#include "render/actor_producers.h"

class Core;

namespace ts2 {

// Everything one Core owns; per-Core guest state lives in guest RAM, not here.
struct ToyStory2Context {
  GuestMainBoot guestMainBoot;
  GraphicsSync graphicsSync;
  LevelStartPresentation levelStartPresentation;
  OverlayImages overlays;
  audio::SoundBankProcessor soundBank;
  PadOwner pad;
  fmv::GuestMoviePlayer moviePlayer;
  FrameCut frameCut;
  ActorIncarnations actorIncarnations;
  ActorProducers actorProducers;
  // True while a guest call suspended between display fields (the front-end poll) runs; its field
  // barrier then exits the executor at the field boundary so the host can present.
  bool yieldAtFieldBarrier = false;
};

ToyStory2Context &context(Core &core);

} // namespace ts2
