#pragma GCC optimize("Os", "no-ipa-sra")
#include <Arduino.h>
#include <string.h>
#include <CHGfx.h>
#include "../../config.h"
#include "Stage.h"
#include "../CHGame.h"
#include "../gfx/Palette.h"
#include "../gfx/Draw.h"
#include "../gfx/Fmt.h"
#include "../fx/Fx.h"
#include "../audio/Audio.h"
#include "../ai/Ai.h"
#include "../assets/Assets.h"

namespace stage {

using wd::SIZE;

game::Play tent;
uint8_t tentSlot[wd::RACK];
int16_t tentScore;
uint8_t cursor, rackSel, pickSel, swapMarks, mode, viewSide;
bool down;

// Layout.
static const int BX = 4, BY = 9, CELL = 8, VIEW_H = 96;      // the board's window: 15 x 12 squares
static const int RACK_X = 2, RACK_Y = 109, TILE_W = 13, TILE_H = 17, TILE_PITCH = 15;

static uint8_t scrollY, scrollTo;       // px: 0..24

// The show.
enum Anim : uint8_t { A_NONE, A_DROP, A_SWEEP, A_FLOAT, A_NOTE };
static uint8_t anim, animT;
static uint8_t dropped;                 // of the CPU's tiles, how many have landed
static int16_t shown[2];                // the scores on the HUD, counting up
static int16_t floatX, floatY;          // the "+34" rising off the word
static char floatText[6];
static uint8_t floatT;

static char noteText[32];
static uint8_t noteCol, noteT;
static const char *cpuThinking;
static uint8_t denyT;
static wd::Span denySpan;
static bool denyWord;

static bool dirty = true;

// ---------------------------------------------------------------------------
static inline int cellX(uint8_t cell) { return BX + (cell % SIZE) * CELL; }
static inline int cellY(uint8_t cell) { return BY + (cell / SIZE) * CELL - scrollY; }

void follow(uint8_t cell) {
    int row = cell / SIZE, top = scrollTo / CELL;
    if (row < top + 1) top = row - 1;
    if (row > top + 10) top = row - 10;
    if (top < 0) top = 0;
    if (top > 3) top = 3;
    scrollTo = (uint8_t)(top * CELL);
}

bool slotFree(uint8_t slot) {
    if (!game::rack[viewSide][slot]) return false;
    for (uint8_t i = 0; i < tent.n; i++) if (tentSlot[i] == slot) return false;
    return true;
}

void retally() {
    wd::Result r;
    tentScore = tent.n && wd::check(game::board, tent.p, tent.n, r) == wd::OK ? r.score : -1;
    dirty = true;
}

void begin() {}

void newGame() {
    tent.n = 0;
    tentScore = -1;
    cursor = wd::CENTRE;
    rackSel = 0;
    swapMarks = 0;
    mode = BOARD;
    down = false;
    viewSide = game::setup.mode == game::VS_CPU ? 0 : game::turn;
    anim = A_NONE;
    noteT = denyT = floatT = 0;
    cpuThinking = nullptr;
    shown[0] = game::score[0];
    shown[1] = game::score[1];
    scrollY = scrollTo = 8;
    follow(cursor);
    scrollY = scrollTo;
    dirty = true;
}

void note(const char *text, uint8_t colour, uint8_t frames) {
    strncpy(noteText, text, sizeof noteText - 1);
    noteCol = colour;
    noteT = frames;
    dirty = true;
}

void thinking(const char *text) {
    cpuThinking = text;
    dirty = true;
}

void deny(const wd::Span *word) {
    denyT = 36;
    denyWord = word != nullptr;
    if (word) denySpan = *word;
    fx::shake(8, 2);
    audio::sfx(Sfx::Deny);
}

// ---------------------------------------------------------------------------
// The show
// ---------------------------------------------------------------------------
static void sparkle(uint8_t cell, fx::Kind k, uint8_t n, uint8_t colour) {
    fx::burst(k, cellX(cell) + 3, cellY(cell) + 3, n, 22, colour);
}

void show(bool byCpu) {
    const game::Last &l = game::last;
    tent.n = 0;
    tentScore = -1;
    mode = BOARD;
    animT = 0;
    dirty = true;
    if (l.kind == game::PLAYED) {
        follow((uint8_t)(l.main.start + (l.main.len / 2) * l.main.step));
        dropped = byCpu ? 0 : l.n;
        anim = byCpu ? A_DROP : A_SWEEP;
        return;
    }
    char *p = fmtStr(noteText, game::setup.mode == game::VS_CPU ? (l.side ? "CPU " : "YOU ") : (l.side ? "PLAYER 2 " : "PLAYER 1 "));
    if (l.kind == game::SWAPPED) {
        p = fmtInt(fmtStr(p, game::setup.mode == game::VS_CPU && !l.side ? "SWAP " : "SWAPS "), l.n);
        fmtStr(p, l.n == 1 ? " TILE" : " TILES");
        audio::sfx(Sfx::Whoosh);
    } else {
        fmtStr(p, game::setup.mode == game::VS_CPU && !l.side ? "PASS" : "PASSES");
        audio::sfx(Sfx::NoMove);
    }
    noteCol = SILVER;
    noteT = 80;
    anim = A_NOTE;
}

bool busy() { return anim != A_NONE || denyT; }

void update() {
    if (scrollY != scrollTo) { scrollY = (uint8_t)(scrollY < scrollTo ? scrollY + 2 : scrollY - 2); dirty = true; }
    if (noteT && !--noteT) dirty = true;
    if (denyT && !--denyT) dirty = true;
    if (floatT) { floatT--; dirty = true; }
    for (uint8_t s = 0; s < 2; s++) {
        int16_t d = (int16_t)(game::score[s] - shown[s]);
        if (!d || anim == A_DROP || anim == A_SWEEP) continue;
        int16_t stepBy = (int16_t)(d > 24 || d < -24 ? 3 : 1);
        shown[s] = (int16_t)(shown[s] + (d > 0 ? stepBy : -stepBy));
        if (!(shown[s] & 3)) audio::sfx(Sfx::Tick);
        dirty = true;
    }
    const game::Last &l = game::last;
    switch (anim) {
        case A_DROP:
            // The CPU's tiles come down one after another.
            if (scrollY != scrollTo) break;
            if (++animT >= 7) {
                animT = 0;
                sparkle(l.cell[dropped], fx::DUST, 5, FELT_LT);
                fx::shake(3, 1);
                audio::sfx(Sfx::Land);
                if (++dropped >= l.n) anim = A_SWEEP;
            }
            dirty = true;
            break;
        case A_SWEEP:
            // The word lights up a letter at a time, then pays.
            dirty = true;
            if (++animT < l.main.len * 3 + 6) {
                if (animT % 3 == 1) audio::sfx(Sfx::Tock);
                break;
            }
            {
                uint8_t mid = (uint8_t)(l.main.start + (l.main.len / 2) * l.main.step);
                char *p = floatText;
                *p++ = '+';
                fmtInt(p, l.score);
                floatX = (int16_t)(cellX(mid) + 4);
                floatY = (int16_t)(cellY(mid) - 4);
                floatT = 50;
                if (l.n == wd::RACK) {
                    fx::banner("BINGO!", fx::B_RAINBOW, 40, 90);
                    fx::fountain(40, 100, 18);
                    fx::fountain(88, 100, 18);
                    audio::sfx(Sfx::Doubles);
                    audio::led(audio::LED_PARTY);
                } else if (l.score >= 30) {
                    for (uint8_t i = 0; i < l.n; i++) sparkle(l.cell[i], fx::STAR, 4, GOLD);
                    audio::sfx(Sfx::Pickup);
                    audio::led(audio::LED_BLINK);
                } else {
                    sparkle(mid, fx::SPARK, 8, GOLD);
                    audio::sfx(Sfx::Coin);
                }
            }
            anim = A_FLOAT;
            animT = 0;
            break;
        case A_FLOAT:
            if (!floatT && !fx::bannerActive() && shown[0] == game::score[0] && shown[1] == game::score[1]) anim = A_NONE;
            break;
        case A_NOTE:
            if (!noteT) anim = A_NONE;
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static const uint8_t RM_ID[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const uint8_t PREMIUM_COL[5] = {FELT, CYAN, BLUE, WINE, RED};

// A tile on the board: 7 x 7 with a shaded edge, its letter in the 3x5 font.
static void boardTile(int x, int y, uint8_t v, uint8_t face) {
    gfx_fillRect(x, y, 6, 6, face);
    gfx_hline(x, y + 6, 7, WOOD);
    gfx_vline(x + 6, y, 6, WOOD);
    glyph(x + 1, y, FONT35['A' + (v & wd::LETTER) - 1 - FONT35_FIRST], 3, (v & wd::BLANK) ? RED : INK);
}

static bool inSpan(const wd::Span &s, uint8_t cell, uint8_t upTo) {
    for (uint8_t i = 0; i < s.len && i < upTo; i++) if ((uint8_t)(s.start + i * s.step) == cell) return true;
    return false;
}

// play: the game's view (the HUD and the rack go over its edges). Else the
// whole board, top to bottom: the title's backdrop.
static void drawBoard(bool play) {
    int y0 = play ? BY - 7 : 0, y1 = play ? BY + VIEW_H + 7 : 128;
    gfx_fillRect(BX, y0, 120, y1 - y0, FELT);
    for (int c = 0; c < SIZE; c++) gfx_vline(BX + c * CELL + 7, y0, y1 - y0, FELT_DK);
    gfx_fillRect(0, y0, BX, y1 - y0, WOOD);
    gfx_fillRect(BX + 120, y0, 4, y1 - y0, WOOD);
    gfx_vline(BX - 1, y0, y1 - y0, GOLD);
    gfx_vline(BX + 120, y0, y1 - y0, GOLD);
    const game::Last &l = game::last;
    bool lastPlay = play && l.kind == game::PLAYED;
    for (uint8_t row = (uint8_t)(scrollY / CELL); row < SIZE; row++) {
        int y = BY + row * CELL - scrollY;
        if (y >= (play ? BY + VIEW_H : 128)) break;
        gfx_hline(BX, y + 7, 120, FELT_DK);
        for (uint8_t col = 0; col < SIZE; col++) {
            uint8_t cell = (uint8_t)(row * SIZE + col), v = game::board[cell];
            int x = BX + col * CELL;
            uint8_t face = SKIN;
            int lift = 0;
            if (lastPlay && v) {
                for (uint8_t i = 0; i < l.n; i++) {
                    if (l.cell[i] != cell) continue;
                    if (i > dropped) v = 0;                                     // not down yet
                    else if (i == dropped && anim == A_DROP) lift = 6 - animT;  // on its way
                    face = GOLD;                                                // the last play stays marked
                }
                if (anim == A_SWEEP && inSpan(l.main, cell, (uint8_t)(animT / 3 + 1))) face = FX_B;
            }
            if (play) for (uint8_t i = 0; i < tent.n; i++)
                if (tent.p[i].cell == cell) { v = tent.p[i].tile; face = WHITE; }
            if (denyT && (denyT & 4) && (denyWord ? inSpan(denySpan, cell, 15) : face == WHITE)) face = RED;
            if (!v || lift) {
                uint8_t pr = wd::premium(cell);
                if (pr) gfx_fillRect(x, y, 7, 7, PREMIUM_COL[pr]);
                if (cell == wd::CENTRE) glyph(x + 2, y + 1, FONT35['*' - FONT35_FIRST], 3, GOLD);
            }
            if (v) boardTile(x, y - lift, v, face);
        }
    }
}

void renderBoard() {
    scrollY = scrollTo = 8;             // (the top row is under the title's sign)
    drawBoard(false);
}

// A tile in the rack: its letter twice the size, its value in the corner.
static void rackTile(int x, int y, uint8_t t, uint8_t face) {
    gfx_fillRect(x, y, TILE_W - 1, TILE_H - 1, face);
    gfx_hline(x, y + TILE_H - 1, TILE_W, WOOD);
    gfx_vline(x + TILE_W - 1, y, TILE_H - 1, WOOD);
    if (t == wd::BLANK_TILE) return;
    char s[3] = {(char)('A' + t - 1), 0, 0};
    text35x2(x + 1, y + 1, s, INK);
    uint8_t val = wd::VALUE[t];
    fmtInt(s, val);
    text35(x + (val >= 10 ? 4 : 8), y + 11, s, WOOD);
}

static void drawRack(uint32_t frame) {
    gfx_fillRect(0, 106, 128, 22, INK);
    gfx_hline(0, 105, 128, GOLD);
    bool mine = !game::cpuTurn() && !game::over;
    for (uint8_t i = 0; i < wd::RACK; i++) {
        uint8_t t = game::rack[viewSide][i];
        int x = RACK_X + i * TILE_PITCH, y = RACK_Y;
        if (!t || !slotFree(i)) { gfx_rect(x + 2, y + 3, TILE_W - 4, TILE_H - 6, NAVY); continue; }     // an empty slot
        uint8_t face = SKIN;
        bool sel = mine && (mode == RACK || mode == SWAP) && i == rackSel;
        if (mode == SWAP && (swapMarks >> i & 1)) { y -= 2; face = CYAN; }
        if (sel) y -= 1;
        rackTile(x, y, t, face);
        if (sel) gfx_rect(x - 1, y - 1, TILE_W + 2, TILE_H + 2, (frame & 16) ? FX_B : WHITE);
    }
    // Beside the rack: what the laid-out tiles would score, and the way the word runs.
    char buf[8];
    if (tent.n) {
        if (tentScore >= 0) { buf[0] = '+'; fmtInt(buf + 1, tentScore); }
        else fmtStr(buf, "--");
        text35(109, 109, buf, tentScore >= 0 ? FX_B : RED);
    }
    if (mine && mode != SWAP) text35(109, 118, down ? "DOWN" : "RIGHT", FELT_LT);
}

static void drawHud() {
    gfx_fillRect(0, 0, 128, 8, INK);
    gfx_hline(0, 8, 128, GOLD);
    char buf[16], *p;
    bool cpu = game::setup.mode == game::VS_CPU;
    for (uint8_t s = 0; s < 2; s++) {
        p = fmtStr(buf, cpu ? (s ? "CPU " : "YOU ") : (s ? "P2 " : "P1 "));
        fmtInt(p, shown[s]);
        uint8_t c = !game::over && game::turn == s ? FX_B : SILVER;
        text35(s ? 126 - text35Width(buf) : 2, 1, buf, c);
    }
    fmtInt(fmtStr(buf, "BAG "), game::bagLeft);
    text35(64 - text35Width(buf) / 2, 1, buf, FELT_LT);
}

static void plate(const char *s, uint8_t c, int y) {
    int w = text35Width(s);
    fillRound(64 - w / 2 - 5, y, w + 10, 11, 3, NAVY);
    roundRect(64 - w / 2 - 5, y, w + 10, 11, 3, GOLD);
    text35(64 - w / 2, y + 3, s, c);
}

static void drawCursor(uint32_t frame) {
    if (game::cpuTurn() || game::over || anim != A_NONE || mode == SWAP) return;
    int x = cellX(cursor), y = cellY(cursor);
    gfx_rect(x - 1, y - 1, 9, 9, (frame & 16) ? FX_B : WHITE);
    if (mode == RACK || mode == PICK) {
        // Where the next tile goes, and which way the word runs from it.
        if (!game::board[cursor]) glyph(x + 2, y + 1, FONT35[(down ? 'V' : '>') - FONT35_FIRST], 3, FX_B);
        // The glove, over the chosen tile (unless it would hide the square).
        if (mode == RACK && y < 84) {
            int bob = (frame >> 4) & 1;
            sprite4(HAND, RACK_X + rackSel * TILE_PITCH + TILE_W / 2 - HAND_TIP, RACK_Y - 18 - bob, RM_ID);
        }
    }
}

static void drawPicker(uint32_t frame) {
    fillRound(11, 30, 106, 62, 3, NAVY);
    roundRect(11, 30, 106, 62, 3, GOLD);
    text35(64 - text35Width("THE BLANK IS...") / 2, 34, "THE BLANK IS...", GOLD);
    for (uint8_t i = 0; i < 26; i++) {
        int x = 16 + (i % 7) * 14, y = 43 + (i / 7) * 12;
        char s[2] = {(char)('A' + i), 0};
        if (i == pickSel) gfx_fillRect(x - 2, y - 1, 11, 12, (frame & 16) ? FX_B : GOLD);
        text35x2(x, y, s, i == pickSel ? INK : WHITE);
    }
}

void invalidate() { dirty = true; }

static uint32_t lastSig;
static bool wasMoving;

bool render(uint32_t frame, uint32_t ui) {
    int lo, hi;
    bool moving = fx::activeRows(lo, hi) || anim != A_NONE || denyT;
    if (wasMoving && !moving) dirty = true;         // once more, to clear up after it
    wasMoving = moving;
    // Still: the blinks step every 16 frames, and the palette does the rest.
    uint32_t sig = (frame >> 4) ^ (ui << 8) ^ ((uint32_t)cursor << 16) ^ ((uint32_t)mode << 24) ^ ((uint32_t)rackSel << 27) ^
                   ((uint32_t)pickSel * 2654435761u) ^ ((uint32_t)swapMarks << 3) ^ (cpuThinking ? ai::progress() >> 3 : 0) ^
                   ((uint32_t)down << 30);
    if (!moving && !dirty && sig == lastSig) return false;
    lastSig = sig;
    dirty = false;
    drawBoard(true);
    drawCursor(frame);
    fx::drawParticles(2);
    if (floatT) {
        int y = floatY - (50 - floatT) / 3, x = floatX - text35x2Width(floatText) / 2;
        if (x < 2) x = 2;
        if (x > 126 - text35x2Width(floatText)) x = 126 - text35x2Width(floatText);
        if (y < 12) y = 12;
        if (floatT > 6 || (floatT & 1)) {
            text35x2(x + 1, y + 1, floatText, INK);
            text35x2(x, y, floatText, FX_B);
        }
    }
    drawHud();
    drawRack(frame);
    if (cpuThinking) {
        plate(cpuThinking, SILVER, 90);
        gfx_hline(36, 99, (ai::progress() * 56) >> 8, FX_B);
    } else if (noteT) plate(noteText, noteCol, 90);
    if (mode == PICK) drawPicker(frame);
    fx::drawBanner();
    fx::applyShake(9, 104);
    return true;
}

}  // namespace stage
