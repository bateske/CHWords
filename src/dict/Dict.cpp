#pragma GCC optimize("Os", "no-ipa-sra")
// The card's side of the dictionary: the file WORDS.DIC in the card's root
// folder (tools/dict/build_sd.py writes it, and describes it).
//
// The file is a hash table of 512-byte blocks, so a word is one block read:
// block 0 is a header, block 1 + (hash & mask) holds every word with that
// hash, each as its letters (1..26) with bit 7 set on the last, a 0 after
// the last word. The last two bytes of every block check the rest.
#include <string.h>
#include <CHGfx.h>
#include "Dict.h"
#include "DictData.h"
#include "../sd/SdSpi.h"
#ifdef CHSIM
#include <sim.h>
#else
#include "../sd/Fat.h"
#endif

namespace dict {

struct Extent { uint32_t lba, blocks; };
static const uint8_t MAX_RUNS = 6;          // a file in more pieces than this is not used
static Extent run[MAX_RUNS];
static uint8_t nRuns;
static bool live;
static uint32_t nWords, seed, mask;

// Block k of the file, checked, in the chunk scratch; nullptr if the card
// did not deliver it.
static const uint8_t *block(uint32_t k) {
    uint8_t *buf = gfx_chunkScratch();
    for (uint8_t i = 0; i < nRuns; i++) {
        if (k >= run[i].blocks) { k -= run[i].blocks; continue; }
        if (!sd::read(run[i].lba + k, buf)) return nullptr;
        uint16_t c = 0;
        for (uint16_t j = 0; j < 510; j++) c = (uint16_t)(c * 31 + buf[j]);
        return c == (uint16_t)(buf[510] | buf[511] << 8) ? buf : nullptr;
    }
    return nullptr;
}

static bool openFile() {
#ifdef CHSIM
    uint32_t blocks = sim_cardBlocks();
    run[0] = {0, blocks};
    nRuns = 1;
    return blocks != 0;
#else
    uint8_t *buf = gfx_chunkScratch();
    fat::File f;
    if (!sd::init() || fat::mount(buf) || fat::find("WORDS   DIC", f, buf)) return false;
    static_assert(sizeof(Extent) == sizeof(fat::Run), "the run list is fat's");
    int8_t n = fat::runs(f, (fat::Run *)run, MAX_RUNS, buf);
    nRuns = n > 0 ? (uint8_t)n : 0;
    return n > 0;
#endif
}

void begin() {
    live = false;
    if (!openFile()) return;
    const uint8_t *h = block(0);
    if (!h || memcmp(h, "CHWD\x01", 5) || h[5] > 15) return;
    mask = (1ul << h[5]) - 1;
    memcpy(&nWords, h + 8, 4);
    memcpy(&seed, h + 12, 4);
    live = true;
}

bool card() { return live; }
uint32_t count() { return live ? nWords : DICT_WORDS; }

bool has(const uint8_t *w, uint8_t n) {
    if (!live) return hasCore(w, n);
    uint32_t h = seed;
    for (uint8_t i = 0; i < n; i++) h = (h ^ w[i]) * 16777619u;     // FNV-1a
    const uint8_t *p = block(1 + ((h >> 8) & mask));
    if (!p) {
        // The card has gone: the flash list from here on.
        live = false;
        return hasCore(w, n);
    }
    for (const uint8_t *end = p + 510; p < end && *p;) {
        uint8_t i = 0;
        bool same = true;
        for (;;) {
            uint8_t c = *p++;
            same &= i < n && w[i] == (c & 0x7F);
            i++;
            if (c & 0x80) break;
        }
        if (same && i == n) return true;
    }
    return false;
}

}  // namespace dict
