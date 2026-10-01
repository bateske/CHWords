// CHWords build switches.
//
// Keep feature switches here rather than in --build-property flags. The game
// needs the CHGame core 0.2.4+ and its default Peripherals setting ("Game",
// which compiles out Serial1/tone/HardwareTimer: ~4 KB of flash). It is
// built with Optimize set to "Smallest + LTO" and, for release, USB set to
// "Upload only" (no Serial: ~0.6 KB); it fits without either.
#pragma once

#define CHWD_VERSION     "0.1"

// Serial debug protocol: screenshots, input injection, lockstep, perf.
// Off in normal builds. tools/device.py turns it on with
// --build-property build.extra_flags, and leaves USB at "Serial" for it.
#ifndef CHWD_DEBUG
#ifdef CHSIM
#define CHWD_DEBUG       1       // the simulator is driven through the protocol
#else
#define CHWD_DEBUG       0
#endif
#endif

// Device debug builds carry the ~2 KB protocol and USB Serial, so they
// leave out what the measurements made with them never need: saving, the
// options and setup screens and the hint (games are started with the
// protocol's G command). The simulator (not flash-bound) and
// release builds keep everything.
#if CHWD_DEBUG && !defined(CHSIM) && !defined(CHWD_FULL)
#define CHWD_LEAN        1
#else
#define CHWD_LEAN        0
#endif

// Section profiler (dbg::prof + the T command). Opt-in: costs flash.
#ifndef CHWD_PROFILE
#define CHWD_PROFILE     0
#endif

// Frame rate the game logic is paced for.
#define CHWD_FPS         60
