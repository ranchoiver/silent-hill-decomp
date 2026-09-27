#undef NDEBUG
#include "game.h"
#include "bodyprog/sys/joy.h"
#include "pc_config.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

s_SysWork g_SysWork;
s_GameWork g_GameWork;
s_GameWork* const g_GameWorkPtr = &g_GameWork;
s_PcConfig g_PcConfig;
static s_ControllerData controller;
s_ControllerData* const g_Controller0 = &controller;
int g_DebugThirdPersonCam = 1, g_PcFpsCam, g_PcConsoleInputActive;
int g_PcQuickOptionsActive, g_PcRearLookActive, g_PcBindsGen;
static Uint8 keyboard[SDL_NUM_SCANCODES];
static int padHeld;

const Uint8* SDL_GetKeyboardState(int* count) {
    if (count) *count = SDL_NUM_SCANCODES;
    return keyboard;
}
int PsyX_LookupGameControllerMapping(const char* name, int fallback) {
    return strcmp(name,"rightshoulder") == 0 ? 3 : fallback;
}
int PsyX_RawControllerButtonHeld(int button) {
    assert(button == 3);
    return padHeld;
}

/* FUNCTION_UNDER_TEST */

static void expect(int active) {
    Pc_RearLookUpdate();
    assert(g_PcRearLookActive == active);
}

int main(void) {
    strcpy(g_PcConfig.altcam.keyRearLook,"B");
    strcpy(g_PcConfig.altcam.padRearLook,"rightshoulder");
    g_GameWork.gameState = GameState_InGame;
    g_SysWork.sysState = SysState_Gameplay;
    g_GameWork.config.controllerConfig.aim = ControllerFlag_R2;
    for (int pad = 0; pad < 2; ++pad) {
        keyboard[SDL_SCANCODE_B] = !pad;
        padHeld = pad;
        expect(1);
        /* Rear look first, then aim: suppress before combat enters aiming. */
        controller.heldBtnFlags = ControllerFlag_R2;
        expect(0);
        g_SysWork.playerCombat.isAiming = true;
        expect(0);
        /* Release aim while the combat state is still lowering the weapon. */
        controller.heldBtnFlags = 0;
        expect(0);
        g_SysWork.playerCombat.isAiming = false;
        expect(1);
        /* Aim first, then rear look; releasing rear look cannot cancel aim. */
        keyboard[SDL_SCANCODE_B] = padHeld = 0;
        controller.heldBtnFlags = ControllerFlag_R2;
        expect(0);
        keyboard[SDL_SCANCODE_B] = !pad;
        padHeld = pad;
        expect(0);
        assert(controller.heldBtnFlags == ControllerFlag_R2);
        controller.heldBtnFlags = 0;
        expect(1);
        g_PcConsoleInputActive = 1; expect(0);
        g_PcConsoleInputActive = 0; expect(1);
        g_PcQuickOptionsActive = 1; expect(0);
        g_PcQuickOptionsActive = 0; expect(1);
        g_PcFpsCam = 1; expect(0);
        g_PcFpsCam = 0; expect(1);
        g_DebugThirdPersonCam = 0; expect(0);
        g_DebugThirdPersonCam = 1;
    }
    /* Aim mask follows the user's mapping, not a hardcoded R2 button. */
    g_GameWork.config.controllerConfig.aim = ControllerFlag_L2;
    controller.heldBtnFlags = ControllerFlag_R2; expect(1);
    controller.heldBtnFlags = ControllerFlag_L2; expect(0);
    controller.heldBtnFlags = 0; expect(1);
    /* A live rear-look rebind drops the old key and accepts the new one. */
    padHeld = 0;
    keyboard[SDL_SCANCODE_B] = 1;
    strcpy(g_PcConfig.altcam.keyRearLook,"N");
    ++g_PcBindsGen; expect(0);
    keyboard[SDL_SCANCODE_N] = 1; expect(1);
    puts("PASS: rear-look aim transitions, keyboard/controller, camera modes and rebinding");
}
