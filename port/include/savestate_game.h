#ifndef _IN_SAVESTATE_GAME_H
#define _IN_SAVESTATE_GAME_H

// Force-included into every game (src/) translation unit by CMakeLists.txt.
//
// 1. Every mutable global and static the decompiled game defines is placed in two
//    dedicated sections, so the checkpoint code (port/src/savestate.c) can snapshot the
//    game's state as two flat ranges without touching the port's own globals (SDL, GL,
//    OpenXR, audio backend, net play), which live in the ordinary .data/.bss.
// 2. The game's few direct heap allocations (room geometry, VR gun copies) are routed
//    into a checkpoint arena, so the pointers held by those globals stay valid on restore.
//
// Only clang on ELF (Android/Linux) and Mach-O (macOS) is supported; elsewhere the
// header is a no-op and checkpoints report themselves unsupported.

#if defined(__clang__) && !defined(__cplusplus) && (defined(__ELF__) || defined(__APPLE__))

#define PD_SAVESTATE_SECTIONS 1

#ifdef __APPLE__
#pragma clang section data="__DATA,__pd_gdata" bss="__DATA,__pd_gbss"
#else
#pragma clang section data="pd_gdata" bss="pd_gbss"
#endif

#define sysMemAlloc savestateArenaAlloc
#define sysMemZeroAlloc savestateArenaZeroAlloc
#define sysMemRealloc savestateArenaRealloc
#define sysMemFree savestateArenaFree

#endif

#endif
