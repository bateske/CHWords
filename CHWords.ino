// CHWords - a casino crossword tile game for the CHGame handheld (CH32X035,
// 128x128 ST7735, piezo, microSD), in the look of CHBlackjack and its tables.
//
// The rules are in src/rules, the dictionary (a compressed list in flash,
// and the full one on the SD card) in src/dict and src/sd, the CPU in
// src/ai, the game's flow in src/game, and everything you see and hear in
// src/stage and src/states.
#include "config.h"
#include <CHGfx.h>
#include "src/CHGame.h"
#include "src/Frame.h"

void setup() {
    arduboy.boot();
    gfx_begin(GFX_DIV2, GFX_12BPP);
    frame::begin();
    arduboy.setFrameRate(CHWD_FPS);
}

void loop() {
    frame::run();
}
