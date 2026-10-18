// Host test for the checkpoint arena (port/src/savestate_arena.c).
// Build + run: tools/savestate-test/run.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <PR/ultratypes.h>
#include "savestate.h"

void *sysMemAlloc(const u32 size) { return malloc(size); }
void *sysMemZeroAlloc(const u32 size) { return calloc(1, size); }
void *sysMemRealloc(void *ptr, const u32 size) { return realloc(ptr, size); }
void sysMemFree(void *ptr) { free(ptr); }
void sysLogPrintf(s32 level, const char *fmt, ...) { (void)level; (void)fmt; }

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

#define N 512
static u8 *ptrs[N];
static u32 sizes[N];

static void fill(int i) { memset(ptrs[i], (u8)(i * 7 + 1), sizes[i]); }

static int intact(int i)
{
	for (u32 j = 0; j < sizes[i]; j++) {
		if (ptrs[i][j] != (u8)(i * 7 + 1)) return 0;
	}
	return 1;
}

int main(void)
{
	const size_t empty = savestateArenaTop();
	CHECK(savestateArenaBase() != NULL, "arena did not initialise");

	// random alloc/free/realloc stress against each block's expected contents
	srand(1234);
	for (int iter = 0; iter < 200000; iter++) {
		const int i = rand() % N;
		if (ptrs[i]) {
			CHECK(intact(i), "block %d corrupted at iter %d", i, iter);
			if (rand() % 4 == 0) {
				const u32 ns = 1 + rand() % 20000;
				const u32 keep = ns < sizes[i] ? ns : sizes[i];
				ptrs[i] = savestateArenaRealloc(ptrs[i], ns);
				for (u32 j = 0; j < keep; j++) CHECK(ptrs[i][j] == (u8)(i * 7 + 1), "realloc lost data");
				sizes[i] = ns;
				fill(i);
			} else {
				savestateArenaFree(ptrs[i]);
				ptrs[i] = NULL;
			}
		} else {
			sizes[i] = 1 + rand() % 20000;
			ptrs[i] = rand() % 2 ? savestateArenaAlloc(sizes[i]) : savestateArenaZeroAlloc(sizes[i]);
			CHECK(ptrs[i] != NULL, "alloc failed");
			CHECK(((uintptr_t)ptrs[i] % 16) == 0, "misaligned block");
			CHECK(ptrs[i] >= savestateArenaBase() && ptrs[i] + sizes[i] <= savestateArenaBase() + savestateArenaTop(), "block outside arena");
			fill(i);
		}
	}
	for (int i = 0; i < N; i++) {
		if (ptrs[i]) {
			CHECK(intact(i), "block %d corrupted at end", i);
			savestateArenaFree(ptrs[i]);
			ptrs[i] = NULL;
		}
	}
	CHECK(savestateArenaTop() == empty, "arena did not shrink back to empty: top %zu, want %zu", savestateArenaTop(), empty);

	// a checkpoint is a byte copy of [base, top); copying it back must restore the allocator too
	u8 *a = savestateArenaAlloc(1000);
	u8 *b = savestateArenaAlloc(2000);
	memset(a, 0xaa, 1000);
	memset(b, 0xbb, 2000);
	const size_t snaptop = savestateArenaTop();
	u8 *snap = malloc(snaptop);
	memcpy(snap, savestateArenaBase(), snaptop);

	savestateArenaFree(a);
	u8 *c = savestateArenaAlloc(5000);
	memset(c, 0xcc, 5000);
	savestateArenaFree(b);

	memcpy(savestateArenaBase(), snap, snaptop);
	CHECK(savestateArenaTop() == snaptop, "restore did not restore top");
	CHECK(a[0] == 0xaa && a[999] == 0xaa && b[0] == 0xbb && b[1999] == 0xbb, "restore did not restore contents");
	u8 *d = savestateArenaAlloc(100);
	CHECK(d >= b + 2000 || d + 100 <= a, "alloc after restore overlaps a live block");
	savestateArenaFree(d);
	savestateArenaFree(a);
	savestateArenaFree(b);
	CHECK(savestateArenaTop() == empty, "arena not empty after restore + frees");
	free(snap);

	// pointers from outside the arena go to the real allocator
	savestateArenaFree(malloc(64));

	if (failures) {
		printf("arena_test: %d FAILURES\n", failures);
		return 1;
	}
	printf("arena_test: PASS\n");
	return 0;
}
