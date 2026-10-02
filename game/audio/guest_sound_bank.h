// guest_sound_bank.h — the guest's VAB sound-bank processor, run under the framework's bound.
#pragma once

class Core;

namespace ts2::audio {

// `FUN_8007F108(bank)`, the routine every asset decode calls to open and transfer one sound bank
// (the five call sites are `0x8003A780`, `0x8003D88C` twice, `0x8003EE4C` and `0x8004171C`, plus
// `0x8007F28C`), DECOMPILED WHOLE from SLUS_008.93, exact bytes, 97 instructions at
// 0x8007F108..0x8007F28B:
//
//     iVar1 = FUN_80082008(&path);                       // normalized search, already natively owned
//     if ((iVar1 + 0x7FF & 0xFFFFF800) < 0x4001) {
//       ... open the VAB head, transfer the VAB body, check the transfer id, report the game's own
//           "SsVabOpenHead failed" / "SsVabTransBody failed" messages on failure, and return ...
//     }
//     do { FUN_80088628(0); break 1; } while (true);     // 0x8007F174
//
// The ELSE leg is the last unbounded wait in the load path. It is not a wait for anything this
// product owes the guest: the delay slot at 0x8007F170 stores zero into `$s0` and the branch at
// 0x8007F180 is `beqz $s0`, so the loop is unconditional — `VSync(0)` returns and the loop repeats
// forever. It is the routine's own assertion that the searched bank fits the window it accepts, and
// retail hangs there exactly as this product would; the sound-bank errors the routine CAN detect
// are the two legs that report through the game's own messages and return.
//
// This owner therefore changes nothing about what the routine decides and skips nothing about what
// it does: the guest's own body still runs, and it runs through the framework's bounded resume loop
// (`callOriginalToReturnResuming`). A processor that returns is untouched, and one that reaches the
// assertion costs bounded display fields and then ends as a loud, named refusal instead of an
// unbounded spin the player would watch forever.
void installSoundBankProcessorOverride(Core &core);

} // namespace ts2::audio