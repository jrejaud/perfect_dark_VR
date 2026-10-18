#include <stdio.h>
#include <ultra64.h>
#include "constants.h"
#include "game/filemgr.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "system.h"
#include "unlockmissions.h"

extern s32 g_NetMode;

// One-shot mission unlock for an existing save.
//
// Put a file named "unlock-missions" in the data folder containing a solo mission number
// N (7 = Area 51: Rescue). The next time a profile is loaded, every mission before N gets a
// best time on Agent, Special Agent and Perfect Agent if it has none, which opens mission N
// on all three difficulties, and the profile's current mission moves up to N. The profile is
// saved through the game's own save routine and the file is renamed to
// "unlock-missions.done", so it runs once.
//
// The best time written is 59:59, slower than every target time, so no time-based cheat
// unlocks as a side effect.

#define UNLOCK_FILE "unlock-missions"
#define UNLOCK_DONE "unlock-missions.done"
#define UNLOCK_BESTTIME (59 * 60 + 59)

static s32 g_UnlockPending = 0;

// Called by the file manager right after a profile has loaded successfully. The save is
// deferred to unlockMissionsTick(), outside the file manager's own operation.
void unlockMissionsOnProfileLoaded(void)
{
	g_UnlockPending = 1;
}

void unlockMissionsTick(void)
{
	FILE *f;
	s32 upto = -1;
	s32 had = 0;
	s32 changed = 0;

	if (!g_UnlockPending) {
		return;
	}
	g_UnlockPending = 0;

	if (g_NetMode != NETMODE_NONE) {
		return;
	}

	f = fopen(UNLOCK_FILE, "r");
	if (!f) {
		return;
	}
	if (fscanf(f, "%d", &upto) != 1) {
		upto = -1;
	}
	fclose(f);

	if (upto < 1 || upto > SOLOSTAGEINDEX_SKEDARRUINS + 1) {
		sysLogPrintf(LOG_NOTE, "unlock-missions: ignoring '%s', want a mission number 1-%d", UNLOCK_FILE, SOLOSTAGEINDEX_SKEDARRUINS + 1);
		return;
	}

	for (s32 s = 0; s < NUM_SOLOSTAGES; s++) {
		for (s32 d = DIFF_A; d <= DIFF_PA; d++) {
			had += g_GameFile.besttimes[s][d] != 0;
		}
	}

	sysLogPrintf(LOG_NOTE, "unlock-missions: profile '%.11s' at mission %d, played %u s, %d best times before",
		g_GameFile.name, g_GameFile.autostageindex, g_GameFile.totaltime, had);

	for (s32 s = 0; s < upto; s++) {
		for (s32 d = DIFF_A; d <= DIFF_PA; d++) {
			if (g_GameFile.besttimes[s][d] == 0) {
				g_GameFile.besttimes[s][d] = UNLOCK_BESTTIME;
				changed++;
			}
		}
	}

	// the agent-select screen and the mission list start on this mission
	if (g_GameFile.autostageindex < upto) {
		g_GameFile.autostageindex = upto;
		changed++;
	}

	if (changed && !filemgrSaveOrLoad(&g_GameFileGuid, FILEOP_SAVE_GAME_000, 0)) {
		sysLogPrintf(LOG_NOTE, "unlock-missions: saving the profile failed (pak error %d), will retry on the next load", g_FilemgrLastPakError);
		return;
	}

	remove(UNLOCK_DONE);
	rename(UNLOCK_FILE, UNLOCK_DONE);
	sysLogPrintf(LOG_NOTE, "unlock-missions: missions 0-%d complete on all difficulties (%d changes), profile saved", upto - 1, changed);
}
