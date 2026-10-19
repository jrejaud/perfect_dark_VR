	// Test trigger, compiled in only by linux-run.sh (never in a real build):
	// PD_CHECKPOINT_TEST="<saveframe>,<loadframe>" takes / loads the checkpoint at those frames.
	{
		static s32 parsed = 0, saveframe = -1, loadframe = -1, saved = 0, loaded = 0, ticks = 0;
		if (!parsed) {
			const char *env = getenv("PD_CHECKPOINT_TEST");
			parsed = 1;
			if (env) {
				sscanf(env, "%d,%d", &saveframe, &loadframe);
			}
		}
		if (g_StageNum < STAGE_TITLE && g_StageNum != STAGE_CITRAINING && saveframe >= 0 && !saved && g_Vars.lvframenum >= saveframe) {
			saved = 1;
			g_SavestatePendingSave = 1;
		}
		if (g_StageNum < STAGE_TITLE && g_StageNum != STAGE_CITRAINING && loadframe >= 0 && !loaded && g_Vars.lvframenum >= loadframe) {
			loaded = 1;
			g_SavestatePendingLoad = 1;
		}
		if (g_StageNum < STAGE_TITLE && g_StageNum != STAGE_CITRAINING && g_Vars.bond && (++ticks % 600) == 0) {
			sysLogPrintf(LOG_NOTE, "savestate-test: frame %d pos=(%.0f,%.0f,%.0f)", g_Vars.lvframenum,
				g_Vars.bond->prop->pos.x, g_Vars.bond->prop->pos.y, g_Vars.bond->prop->pos.z);
		}
	}
