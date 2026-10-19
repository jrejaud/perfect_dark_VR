	// UI trace, compiled in only by linux-ui.sh (never in a real build): logs every menu change
	// as "uitrace: menu <root> <dialog title>" and the current stage, so step files can wait on
	// real events (waitlog) instead of guessing how long the software renderer takes.
	{
		extern char *menuResolveDialogTitle(struct menudialogdef *dialogdef);
		static s32 lastroot = -2, laststage = -2;
		static struct menudialogdef *lastdef = (struct menudialogdef *)-1;
		struct menudialogdef *def = g_Menus[0].curdialog ? g_Menus[0].curdialog->definition : NULL;

		if (g_StageNum != laststage) {
			laststage = g_StageNum;
			sysLogPrintf(LOG_NOTE, "uitrace: stage %d", g_StageNum);
		}
		if (g_MenuData.root != lastroot || def != lastdef) {
			lastroot = g_MenuData.root;
			lastdef = def;
			sysLogPrintf(LOG_NOTE, "uitrace: menu %d %s", g_MenuData.root, def ? menuResolveDialogTitle(def) : "(none)");
		}
	}
	// PD_TEST_KILL_FRAME=<frame>: kill the player once at that frame (death-flow captures)
	{
		static s32 kparsed = 0, killframe = -1, killed = 0;
		extern void playerDie(s32 force);
		if (!kparsed) {
			const char *env = getenv("PD_TEST_KILL_FRAME");
			kparsed = 1;
			if (env) {
				sscanf(env, "%d", &killframe);
			}
		}
		if (g_StageNum < STAGE_TITLE && g_StageNum != STAGE_CITRAINING && killframe >= 0 && !killed && g_Vars.lvframenum >= killframe && g_Vars.bond) {
			killed = 1;
			g_Vars.currentplayer = g_Vars.bond;
			playerDie(true);
			sysLogPrintf(LOG_NOTE, "uitrace: killed the player at frame %d", g_Vars.lvframenum);
		}
	}
