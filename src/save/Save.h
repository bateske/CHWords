// Saving options, lifetime stats and a game in progress.
//
// CHGame has no EEPROM, but its bootloader only erases the flash pages a new
// sketch occupies, so the last pages of the application region survive
// re-uploads. Two pages are used in turn, each record carrying a sequence
// number and a CRC, so a power cut mid-write can only lose the newest save.
// If the sketch ever grows into those pages, saving switches itself off
// rather than overwrite code. (From CHBlackjack, with its own magic: the
// games share the pages, and each ignores the others' records.)
#pragma once
#include <stdint.h>
#include "../game/Game.h"

struct Options {
    uint8_t sound;      // 0 off, 1 on
    uint8_t felt;       // board colour theme (pal::Theme)
    uint8_t level;      // last opponent chosen
    uint8_t pad[5];
};

// Against the CPU.
struct Stats {
    uint16_t won[game::LEVELS], lost[game::LEVELS];
    uint16_t bestGame, bestPlay;        // your highest score in a game, and for one play
};

namespace save {

bool available();                   // false: image too big, or a write failed
bool load(Options &o, Stats &s, bool &hasGame);
bool loadGame();                    // the saved game into game::
// Call after gfx_wait(): the page is built in CHGfx's chunk scratch.
bool store(const Options &o, const Stats &s, bool withGame);

}  // namespace save
