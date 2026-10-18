#include <string.h>
#include <PR/ultratypes.h>
#include "system.h"
#include "savestate.h"

#if defined(__APPLE__) || defined(__unix__) || defined(__ANDROID__)
#include <sys/mman.h>
#endif

// Checkpoint arena that the game's sysMem* calls are redirected to (savestate_game.h).
// Kept apart from savestate.c so it builds without the game headers, for
// tools/savestate-test.

static s32 g_SavestateArenaOverflowed = 0;

//
// First-fit allocator over one reserved region. Its bookkeeping lives inside the region,
// so restoring the region's bytes also restores which blocks are allocated.

#define ARENA_RESERVE (512u * 1024 * 1024)
#define ARENA_ALIGN 16

struct arenahdr {
	size_t top;  // end of the last block, relative to the arena base
	size_t last; // offset of the last block, 0 if none
	u8 pad[ARENA_ALIGN * 2 - 2 * sizeof(size_t)];
};

struct arenablk {
	u32 size;     // whole block including this header
	u32 prevsize; // size of the block before this one, 0 for the first
	u32 used;
	u32 pad;
};

static u8 *g_ArenaBase = NULL;

#define ARENA_HDR ((struct arenahdr *)g_ArenaBase)
#define ARENA_FIRST (sizeof(struct arenahdr))
#define ARENA_BLK(off) ((struct arenablk *)(g_ArenaBase + (off)))

static s32 arenaInit(void)
{
	if (g_ArenaBase) {
		return 1;
	}
#if defined(__APPLE__) || defined(__unix__) || defined(__ANDROID__)
	void *p = mmap(NULL, ARENA_RESERVE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
	if (p == MAP_FAILED) {
		return 0;
	}
	g_ArenaBase = p;
#else
	return 0;
#endif
	ARENA_HDR->top = ARENA_FIRST;
	ARENA_HDR->last = 0;
	return 1;
}

static s32 arenaOwns(const void *ptr)
{
	return g_ArenaBase && (const u8 *)ptr >= g_ArenaBase && (const u8 *)ptr < g_ArenaBase + ARENA_RESERVE;
}

void *savestateArenaAlloc(const u32 size)
{
	const size_t need = ((size + ARENA_ALIGN - 1) & ~(size_t)(ARENA_ALIGN - 1)) + sizeof(struct arenablk);

	if (!arenaInit()) {
		return sysMemAlloc(size);
	}

	for (size_t off = ARENA_FIRST; off < ARENA_HDR->top; off += ARENA_BLK(off)->size) {
		struct arenablk *blk = ARENA_BLK(off);

		if (!blk->used && blk->size >= need) {
			if (blk->size - need >= sizeof(struct arenablk) + ARENA_ALIGN) {
				const size_t restoff = off + need;
				struct arenablk *rest = ARENA_BLK(restoff);
				rest->size = blk->size - need;
				rest->prevsize = need;
				rest->used = 0;
				if (restoff + rest->size < ARENA_HDR->top) {
					ARENA_BLK(restoff + rest->size)->prevsize = rest->size;
				} else {
					ARENA_HDR->last = restoff;
				}
				blk->size = need;
			}
			blk->used = 1;
			return blk + 1;
		}
	}

	if (ARENA_HDR->top + need > ARENA_RESERVE) {
		if (!g_SavestateArenaOverflowed) {
			sysLogPrintf(LOG_NOTE, "savestate: arena full, checkpoints disabled");
		}
		g_SavestateArenaOverflowed = 1;
		return sysMemAlloc(size);
	}

	const size_t off = ARENA_HDR->top;
	struct arenablk *blk = ARENA_BLK(off);
	blk->size = need;
	blk->prevsize = ARENA_HDR->last ? ARENA_BLK(ARENA_HDR->last)->size : 0;
	blk->used = 1;
	ARENA_HDR->last = off;
	ARENA_HDR->top = off + need;
	return blk + 1;
}

void *savestateArenaZeroAlloc(const u32 size)
{
	void *ptr = savestateArenaAlloc(size);
	if (ptr) {
		memset(ptr, 0, size);
	}
	return ptr;
}

void savestateArenaFree(void *ptr)
{
	if (!ptr) {
		return;
	}
	if (!arenaOwns(ptr)) {
		sysMemFree(ptr);
		return;
	}

	size_t off = (u8 *)ptr - g_ArenaBase - sizeof(struct arenablk);
	struct arenablk *blk = ARENA_BLK(off);
	blk->used = 0;

	// merge with the next block
	if (off + blk->size < ARENA_HDR->top) {
		struct arenablk *next = ARENA_BLK(off + blk->size);
		if (!next->used) {
			blk->size += next->size;
			if (off + blk->size < ARENA_HDR->top) {
				ARENA_BLK(off + blk->size)->prevsize = blk->size;
			} else {
				ARENA_HDR->last = off;
			}
		}
	}

	// merge with the previous block
	if (blk->prevsize) {
		const size_t prevoff = off - blk->prevsize;
		struct arenablk *prev = ARENA_BLK(prevoff);
		if (!prev->used) {
			prev->size += blk->size;
			if (prevoff + prev->size < ARENA_HDR->top) {
				ARENA_BLK(prevoff + prev->size)->prevsize = prev->size;
			} else {
				ARENA_HDR->last = prevoff;
			}
			off = prevoff;
			blk = prev;
		}
	}

	// give a free tail back to the bump pointer so snapshots stay small
	if (off + blk->size == ARENA_HDR->top) {
		ARENA_HDR->top = off;
		ARENA_HDR->last = (off > ARENA_FIRST) ? off - blk->prevsize : 0;
	}
}

void *savestateArenaRealloc(void *ptr, const u32 newSize)
{
	if (!ptr) {
		return savestateArenaAlloc(newSize);
	}
	if (!arenaOwns(ptr)) {
		return sysMemRealloc(ptr, newSize);
	}

	const struct arenablk *blk = (const struct arenablk *)ptr - 1;
	const size_t oldSize = blk->size - sizeof(struct arenablk);
	void *newptr = savestateArenaAlloc(newSize);
	if (newptr) {
		memcpy(newptr, ptr, oldSize < newSize ? oldSize : newSize);
		savestateArenaFree(ptr);
	}
	return newptr;
}


u8 *savestateArenaBase(void)
{
	return arenaInit() ? g_ArenaBase : NULL;
}

size_t savestateArenaTop(void)
{
	return arenaInit() ? ARENA_HDR->top : 0;
}

s32 savestateArenaOverflowed(void)
{
	return g_SavestateArenaOverflowed;
}
