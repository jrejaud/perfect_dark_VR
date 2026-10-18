#include <stdio.h>
#include <ultra64.h>
#include "constants.h"
#include "game/filemgr.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "system.h"

// One-shot mission unlock for an existing save.
//
// Put a file named "unlock-missions" in the data folder containing a solo mission number
// (0 = Defection ... 7 = Area 51: Rescue). The next time a profile reaches the Carrington
// Institute hub, every mission BEFORE that one gets a best time on Agent, Special Agent and
// Perfect Agent if it has none, which opens that mission on all three difficulties. The
// profile is saved through the game's own save routine and the file is renamed to
// "unlock-missions.done", so it runs once.
//
// The best time written is 59:59, slower than every target time, so no time-based cheat
// unlocks as a side effect.

#define UNLOCK_FILE "unlock-missions"
#define UNLOCK_DONE "unlock-missions.done"
#define UNLOCK_BESTTIME (59 * 60 + 59)

void unlockMissionsOnStageStart(void)
{
	FILE *f;
	s32 upto = -1;
	s32 changed = 0;

	if (g_StageNum != STAGE_CITRAINING || g_GameFileGuid.fileid == 0) {
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

	for (s32 s = 0; s < upto; s++) {
		for (s32 d = DIFF_A; d <= DIFF_PA; d++) {
			if (g_GameFile.besttimes[s][d] == 0) {
				g_GameFile.besttimes[s][d] = UNLOCK_BESTTIME;
				changed++;
			}
		}
	}

	if (changed) {
		filemgrSaveOrLoad(&g_GameFileGuid, FILEOP_SAVE_GAME_000, 0);
	}

	remove(UNLOCK_DONE);
	rename(UNLOCK_FILE, UNLOCK_DONE);
	sysLogPrintf(LOG_NOTE, "unlock-missions: missions 0-%d complete on all difficulties (%d best times added), profile saved", upto - 1, changed);
}
