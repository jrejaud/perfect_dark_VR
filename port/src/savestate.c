#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "game/hudmsg.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "system.h"
#include "config.h"
#include "savestate.h"

// In-memory mission checkpoint. See savestate.h and savestate_game.h.
//
// The game's whole mutable state is: its globals (two linker sections, see
// savestate_game.h), the memp heap (g_MempHeap, allocated once at boot and never moved)
// and the checkpoint arena below. All three keep their addresses for the life of the
// process, so a byte copy of them is a complete, self-consistent snapshot: every pointer
// inside it is still valid when it is copied back, as long as the stage has not changed.

extern u8 *g_MempHeap;
extern u32 g_MempHeapSize;
extern void gfx_texture_cache_clear(void);
extern s32 g_NetMode;

s32 g_SavestateAutoSave = 0;

static s32 g_SavestatePendingSave = 0;
static s32 g_SavestatePendingLoad = 0;
static s32 g_SavestateValid = 0;

static u8 *g_SavestateBuf = NULL;
static size_t g_SavestateBufCap = 0;
static size_t g_SavestateArenaLen = 0;

// --- game globals ------------------------------------------------------------------------

#ifdef PD_SAVESTATE_SECTIONS
#error "savestate.c must not be built with the game's section pragma"
#endif

#if defined(__clang__) && defined(__APPLE__)
extern u8 g_GameDataStart __asm("section$start$__DATA$__pd_gdata");
extern u8 g_GameDataEnd __asm("section$end$__DATA$__pd_gdata");
extern u8 g_GameBssStart __asm("section$start$__DATA$__pd_gbss");
extern u8 g_GameBssEnd __asm("section$end$__DATA$__pd_gbss");
#define SAVESTATE_HAVE_SECTIONS 1
#define GDATA_START (&g_GameDataStart)
#define GDATA_END (&g_GameDataEnd)
#define GBSS_START (&g_GameBssStart)
#define GBSS_END (&g_GameBssEnd)
#elif defined(__clang__) && defined(__ELF__)
extern u8 __start_pd_gdata[], __stop_pd_gdata[];
extern u8 __start_pd_gbss[], __stop_pd_gbss[];
#define SAVESTATE_HAVE_SECTIONS 1
#define GDATA_START (__start_pd_gdata)
#define GDATA_END (__stop_pd_gdata)
#define GBSS_START (__start_pd_gbss)
#define GBSS_END (__stop_pd_gbss)
#endif

// VR options that live in game files, so a load would otherwise roll them back to what
// they were when the checkpoint was taken. Port-side settings (config registry) are
// handled separately via configValuesCopyOut/In.
extern s32 VrManualReloading, VrlaserDotForALL, VrSeatedMode, VrMotionThrowing;
extern s32 VrMatchCharacterHeight, VrTwoHandAim, VRDebugMtxPos;
extern int VrStickClickToCrouch, VrHideArms, VrFistClench;
extern float VrSetWorldScale, VrPlayerHeight, VrUseSnapTurn;
extern float VrGunOffX, VrGunOffY, VrGunOffZ, VrArmElbowTuck, VrArmBodyFollow;

// Game state that the port defines in its own files (pdmain.c replaces src/lib/main.c), so
// it is outside the game sections; copied into the checkpoint explicitly.
extern s32 g_MainIsEndscreen, g_MainGameLogicEnabled;

static void *const g_SavestateExtraVars[] = {
	&g_MainIsEndscreen, &g_MainGameLogicEnabled,
};

static void *const g_SavestateKeepVars[] = {
	&VrManualReloading, &VrlaserDotForALL, &VrSeatedMode, &VrMotionThrowing,
	&VrMatchCharacterHeight, &VrTwoHandAim, &VRDebugMtxPos,
	&VrStickClickToCrouch, &VrHideArms, &VrFistClench,
	&VrSetWorldScale, &VrPlayerHeight, &VrUseSnapTurn,
	&VrGunOffX, &VrGunOffY, &VrGunOffZ, &VrArmElbowTuck, &VrArmBodyFollow,
};

// --- checkpoint ----------------------------------------------------------------------------

s32 savestateSupported(void)
{
#ifdef SAVESTATE_HAVE_SECTIONS
	return savestateArenaBase() != NULL && !savestateArenaOverflowed();
#else
	return 0;
#endif
}

s32 savestateHasCheckpoint(void)
{
	return g_SavestateValid;
}

static s32 savestateCanSave(void)
{
	return savestateSupported()
		&& g_NetMode == NETMODE_NONE
		&& g_StageNum < STAGE_TITLE
		&& !g_Vars.mplayerisrunning
		&& g_Vars.coopplayernum < 0
		&& g_Vars.antiplayernum < 0
		&& g_Vars.bond
		&& !g_Vars.bond->isdead
		&& !g_Vars.bond->aborted;
}

void savestateRequestSave(void)
{
	g_SavestatePendingSave = 1;
}

void savestateRequestLoad(void)
{
	if (g_SavestateValid) {
		g_SavestatePendingLoad = 1;
	}
}

void savestateOnObjectiveComplete(void)
{
	if (g_SavestateAutoSave) {
		g_SavestatePendingSave = 1;
	}
}

void savestateOnStageStart(void)
{
	g_SavestateValid = 0;
	g_SavestatePendingSave = 0;
	g_SavestatePendingLoad = 0;
}

#ifdef SAVESTATE_HAVE_SECTIONS

static void savestateSave(void)
{
	const size_t datalen = GDATA_END - GDATA_START;
	const size_t bsslen = GBSS_END - GBSS_START;
	const size_t arenalen = savestateArenaTop();
	const size_t total = datalen + bsslen + g_MempHeapSize + arenalen + sizeof(g_SavestateExtraVars) / sizeof(void *) * 4;
	const u64 t0 = sysGetMicroseconds();

	if (total > g_SavestateBufCap) {
		free(g_SavestateBuf);
		g_SavestateBufCap = total + total / 8;
		g_SavestateBuf = malloc(g_SavestateBufCap);
		if (!g_SavestateBuf) {
			g_SavestateBufCap = 0;
			g_SavestateValid = 0;
			sysLogPrintf(LOG_ERROR, "savestate: could not allocate %zu bytes", total);
			return;
		}
	}

	u8 *dst = g_SavestateBuf;
	memcpy(dst, GDATA_START, datalen); dst += datalen;
	memcpy(dst, GBSS_START, bsslen); dst += bsslen;
	memcpy(dst, g_MempHeap, g_MempHeapSize); dst += g_MempHeapSize;
	memcpy(dst, savestateArenaBase(), arenalen); dst += arenalen;
	for (u32 i = 0; i < ARRAYCOUNT(g_SavestateExtraVars); i++, dst += 4) {
		memcpy(dst, g_SavestateExtraVars[i], 4);
	}
	g_SavestateArenaLen = arenalen;
	g_SavestateValid = 1;

	sysLogPrintf(LOG_NOTE, "savestate: saved stage %d frame %d pos=(%.0f,%.0f,%.0f) health=%.2f (data %zu bss %zu heap %u arena %zu) in %u ms",
		g_StageNum, g_Vars.lvframenum, g_Vars.bond->prop->pos.x, g_Vars.bond->prop->pos.y, g_Vars.bond->prop->pos.z, g_Vars.bond->bondhealth, datalen, bsslen, g_MempHeapSize, arenalen, (u32)((sysGetMicroseconds() - t0) / 1000));

	// after the copy, so the message is not part of the checkpoint itself
	hudmsgCreate("Checkpoint Saved", HUDMSGTYPE_DEFAULT);
}

static void savestateLoad(void)
{
	const size_t datalen = GDATA_END - GDATA_START;
	const size_t bsslen = GBSS_END - GBSS_START;
	const u64 t0 = sysGetMicroseconds();

	// things that must survive the load: settings, and the frame clock, which counts real
	// time and would otherwise see the whole gap since the save as one enormous frame
	float keep[ARRAYCOUNT(g_SavestateKeepVars)];
	for (u32 i = 0; i < ARRAYCOUNT(g_SavestateKeepVars); i++) {
		memcpy(&keep[i], g_SavestateKeepVars[i], 4);
	}

	const u32 cfglen = configValuesSize();
	u8 *cfg = malloc(cfglen ? cfglen : 1);
	if (cfg) {
		configValuesCopyOut(cfg);
	}

	const s32 prevframestartt = g_Vars.prevframestartt;
	const s32 thisframestartt = g_Vars.thisframestartt;
	const s32 diffframet = g_Vars.diffframet;
	const s32 lostframetime60t = g_Vars.lostframetime60t;
	const s32 lostframetime240t = g_Vars.lostframetime240t;
	const s32 autosave = g_SavestateAutoSave;

	const u8 *src = g_SavestateBuf;
	memcpy(GDATA_START, src, datalen); src += datalen;
	memcpy(GBSS_START, src, bsslen); src += bsslen;
	memcpy(g_MempHeap, src, g_MempHeapSize); src += g_MempHeapSize;
	memcpy(savestateArenaBase(), src, g_SavestateArenaLen); src += g_SavestateArenaLen;
	for (u32 i = 0; i < ARRAYCOUNT(g_SavestateExtraVars); i++, src += 4) {
		memcpy(g_SavestateExtraVars[i], src, 4);
	}

	for (u32 i = 0; i < ARRAYCOUNT(g_SavestateKeepVars); i++) {
		memcpy(g_SavestateKeepVars[i], &keep[i], 4);
	}

	if (cfg) {
		configValuesCopyIn(cfg);
		free(cfg);
	}

	g_Vars.prevframestartt = prevframestartt;
	g_Vars.thisframestartt = thisframestartt;
	g_Vars.diffframet = diffframet;
	g_Vars.lostframetime60t = lostframetime60t;
	g_Vars.lostframetime240t = lostframetime240t;
	g_SavestateAutoSave = autosave;

	// GPU textures are cached by the address of their source data, which now holds
	// whatever was loaded there at save time
	gfx_texture_cache_clear();

	sysLogPrintf(LOG_NOTE, "savestate: loaded stage %d frame %d pos=(%.0f,%.0f,%.0f) health=%.2f in %u ms", g_StageNum, g_Vars.lvframenum, g_Vars.bond->prop->pos.x, g_Vars.bond->prop->pos.y, g_Vars.bond->prop->pos.z, g_Vars.bond->bondhealth, (u32)((sysGetMicroseconds() - t0) / 1000));

	hudmsgCreate("Checkpoint Loaded", HUDMSGTYPE_DEFAULT);
}

#endif

void savestateTick(void)
{
#ifdef SAVESTATE_HAVE_SECTIONS
	if (g_SavestatePendingLoad) {
		g_SavestatePendingLoad = 0;
		g_SavestatePendingSave = 0;
		if (g_SavestateValid) {
			savestateLoad();
		}
		return;
	}

	if (savestateArenaOverflowed()) {
		g_SavestateValid = 0;
	}

	if (g_SavestatePendingSave) {
		g_SavestatePendingSave = 0;
		if (savestateCanSave()) {
			savestateSave();
		}
	}
#else
	g_SavestatePendingSave = 0;
	g_SavestatePendingLoad = 0;
#endif
}
