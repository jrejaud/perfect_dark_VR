#ifndef VR_SCOPE_H
#define VR_SCOPE_H
/*
 * The VR 3D scope system, one per hand:
 * - vrScopeBegin() fills a slot each game frame for a hand holding a scoped gun
 * - gfx_opengl.cpp draws the recorded world again through the scope's camera
 * - vr_openxr.cpp composites that image as a circular lens quad on the gun's eyepiece
 * Indexed by the game's hand (HAND_RIGHT 0, HAND_LEFT 1).
 */
#include <PR/gbi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GevrScopeState
{
    float vp[16];          /* head camera space to the scope's clip space, column-major */
    float headP[2];        /* the head projection's x and y scales */
    float lens[4];         /* the lens from the hand's grip: right, up, back, diameter (m) */
    float origin[3];       /* the scope camera, camera space (sizes the reticle) */
    float fovDeg;          /* the angle across the lens */
    float gunOrigin[3];    /* gun grip pos in view space (m) */
    float gunAxes[3][3];   /* gun right, up, back axes */
    int   gunValid;        /* whether gunOrigin/gunAxes are valid */
    /*
     * The lens's radius over its distance from the eyes, as last shown
     * (tan of half the angle it fills). vr_openxr.cpp writes it;
     * vrScopeBegin sizes the scope's view to it, so it magnifies as the
     * N64's zoom did.
     */
    float lensTan;
    unsigned int crosshairColor;
    int   crosshairState;  /* 0 = neutral, 1 = target locked (enemy/friendly) */
} GevrScopeState;

extern GevrScopeState gevrScope[2];
extern int gevrScopeOn;   /* this frame's scopes: bit (1 << hand) */
extern int gevrScopeHand[2];

int vrScopeForWeapon(int weaponnum);
void vrScopeBegin(void);
Gfx *vrScopeDrawReticle(Gfx *gdl, int hand);
void vr_update_scope_layers(void);

#ifdef __cplusplus
}
#endif
#endif

