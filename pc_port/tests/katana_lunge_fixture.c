/* Input and world fixture for test_katana_lunge.py. Upper-body state changes
 * and animation cursors are supplied explicitly; collision returns either the
 * complete intended step or zero to test the real displacement handoff. */
#undef NDEBUG
#include "game.h"
#include "bodyprog/bodyprog.h"
#include "bodyprog/player.h"
#include "bodyprog/math/math.h"
#include "bodyprog/sys/joy.h"
#include "pc_config.h"
#include <SDL_scancode.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void TestShim(s_SubCharacter*, s_PlayerExtra*);
s_SysWork g_SysWork;
s_GameWork g_GameWork;
s_GameWork* const g_GameWorkPtr = &g_GameWork;
static s_Savegame savegame;
s_Savegame* const g_SavegamePtr = &savegame;
s_PcConfig g_PcConfig;
int g_DebugThirdPersonCam = 1, g_PcFpsCam, g_TpsCamYaw, g_PcQuickTurnRequest;
static unsigned char keyboard[512];
const unsigned char* g_sdlKeyboardState = keyboard;
static s_ControllerData controller;
s_ControllerData* const g_Controller0 = &controller;
s_AnimInfo HARRY_BASE_ANIM_INFOS[256];
s_MapOverlayHdr* g_pMapOverlayHeader;
q19_12 g_DeltaTime;
u8 D_800AF220;
s8 g_Player_RockDrill_DirectionAttack;
int g_DebugNoWallCollision, g_DebugNoFloorCollision, g_PhantomRejectCount;
FILE* g_ShDebugLog;
static unsigned int now = 1000;
static int upperCalls, footstepCalls, collisionCalls, blocked;
static VECTOR3 attempted;
extern q19_12 D_800C4550;
extern s_800C44F0 D_800C44F0[10];
extern s_800AFBF4 g_Player_EquippedWeaponInfo;
extern q19_12 g_Player_HeadingAngle;
extern u16 g_Player_IsAttacking, g_Player_IsShooting;
void func_8007C0D8(s_SubCharacter*, s_PlayerExtra*, GsCOORDINATE2*);

unsigned int SDL_GetTicks(void) { return now; }
void vwGetViewAngle(SVECTOR* a) { memset(a, 0, sizeof(*a)); }
void vwGetViewPosition(VECTOR3* p) { memset(p, 0, sizeof(*p)); }
q3_12 Math_AngleNormalizeSigned(q3_12 a) { return ((a + 2048) & 4095) - 2048; }
q19_12 Math_Sin(q19_12 a) { return lround(sin(a * 2.0 * M_PI / 4096) * 4096); }
q19_12 Math_Cos(q19_12 a) { return lround(cos(a * 2.0 * M_PI / 4096) * 4096); }
int ratan2(int y, int x) { return lround(atan2(y, x) * 4096 / (2.0 * M_PI)); }
int SquareRoot0(int a) { return sqrt(a); }
s32 Map_SpeedZoneTypeGet(q19_12 x, q19_12 z) { return SpeedZoneType_Normal; }
void Player_MovementStateReset(s_SubCharacter* p, e_PlayerLowerBodyState s) { abort(); }
void Player_UpperBodyUpdate(s_SubCharacter* p, s_PlayerExtra* e) { upperCalls++; }
void func_8007B924(s_SubCharacter* p, s_PlayerExtra* e) {
    footstepCalls++;
    D_800C4550 = p->properties.player.moveSpeed;
}
void Collision_SurfaceGet(s_CollisionSurface* s, q19_12 x, q19_12 z) { memset(s, 0, sizeof(*s)); }
s32 Collision_WallDetect(s_CollisionResult* r, const VECTOR3* offset, s_SubCharacter* p) {
    collisionCalls++;
    attempted = *offset;
    memset(r, 0, sizeof(*r));
    if (!blocked) r->offset = *offset;
    r->ceilingHeight = -Q12(100);
    return 0;
}

int main(void) {
    s_SubCharacter* p = &g_SysWork.playerWork.player;
    s_PlayerExtra* e = &g_SysWork.playerWork.extra;
    const int rates[] = {30, 60, 120, 240};
    const char* moves[] = {"", "W", "S", "A", "D", "WD", "SA", "SR"};
    int scenarios = 0;
    g_GameWork.config.controllerConfig.aim = ControllerFlag_R2;
    g_GameWork.config.controllerConfig.action = ControllerFlag_Cross;
    g_GameWork.config.controllerConfig.run = ControllerFlag_Square;
    controller.analogController.leftX = controller.analogController.leftY = 128;
    D_800C44F0[0].field_4 = 100;
    g_Player_EquippedWeaponInfo.animAttack = ANIM_STATUS(HarryAnim_HandgunRecoil, false);

    for (int fps = 0; fps < 4; fps++)
    for (int camera = 0; camera < 2; camera++)
    for (int attack = 0; attack < 3; attack++)
    for (int move = 0; move < 8; move++)
    for (int releaseAim = 0; releaseAim < 2; releaseAim++)
    for (blocked = 0; blocked < 2; blocked++) {
        memset(&g_SysWork, 0, sizeof(g_SysWork));
        memset(keyboard, 0, sizeof(keyboard));
        now += 1000;
        g_PcFpsCam = camera;
        g_DeltaTime = Q12(1.0f / rates[fps]);
        controller.heldBtnFlags = ControllerFlag_R2;
        if (attack == AttackInputType_Hold) controller.heldBtnFlags |= ControllerFlag_Cross;
        for (const char* c = moves[move]; *c; c++) {
            switch (*c) {
                case 'W': keyboard[SDL_SCANCODE_W] = 1; break;
                case 'S': keyboard[SDL_SCANCODE_S] = 1; break;
                case 'A': keyboard[SDL_SCANCODE_A] = 1; break;
                case 'D': keyboard[SDL_SCANCODE_D] = 1; break;
                case 'R': controller.heldBtnFlags |= ControllerFlag_Square; break;
            }
        }
        g_SysWork.targetNpcIdx = NO_VALUE;
        g_SysWork.playerCombat.weaponAttack = WEAPON_ATTACK(EquippedWeaponId_Katana, attack);
        g_SysWork.playerCombat.isAiming = true;
        e->lowerBodyState = PlayerLowerBodyState_Attack;
        e->upperBodyState = PlayerUpperBodyState_Attack;
        e->model.anim.status = ANIM_STATUS(attack == AttackInputType_Hold ? HarryAnim_Unk29 :
                                          attack == AttackInputType_Multitap ? HarryAnim_Unk30 : HarryAnim_HandgunRecoil, true);
        e->model.anim.keyframeIdx = 106;
        p->model.anim.status = ANIM_STATUS(HarryAnim_Idle, true);
        p->properties.player.moveSpeed = Q12(3.0f); /* stale shim/native handoff */
        int moved = 0;
        for (int frame = 0; frame < rates[fps] / 2; frame++) {
            if (frame == 2) e->model.anim.keyframeIdx = 107;
            e->model.anim.time = Q12(e->model.anim.keyframeIdx) + 123;
            if (frame == 3 && releaseAim) controller.heldBtnFlags &= ~ControllerFlag_R2;
            s_SubCharacter before = *p;
            s_PlayerExtra beforeExtra = *e;
            upperCalls = footstepCalls = collisionCalls = 0;
            g_PcQuickTurnRequest = 1;
            now += 1000 / rates[fps];
            TestShim(p, e);
            assert(upperCalls == 1 && footstepCalls == 1);
            assert(collisionCalls == 0); /* no direct shim strafe displacement */
            assert(memcmp(&p->position, &before.position, sizeof(p->position)) == 0);
            assert(e->lowerBodyState == PlayerLowerBodyState_Attack);
            assert(g_Player_HeadingAngle == 0 && p->properties.player.headingAngle == 0);
            s_SubCharacter actual = *p;
            q19_12 speed = D_800C4550;
            /* Compare every frame to the authored lower-body Attack case with
             * the same current inputs (the upper FSM is intentionally stubbed). */
            *p = before;
            *e = beforeExtra;
            if (p->model.stateStep == 0) p->properties.player.moveSpeed = 0;
            Player_LowerBodyUpdate(p, e);
            assert(speed == D_800C4550);
            assert(actual.model.anim.status == p->model.anim.status);
            assert(actual.model.stateStep == p->model.stateStep);
            assert(actual.model.anim.time == p->model.anim.time);
            assert(actual.model.anim.keyframeIdx == p->model.anim.keyframeIdx);
            *p = actual;
            if (attack == AttackInputType_Hold && frame < 2) assert(speed == 0);
            else if (frame == (attack == AttackInputType_Hold ? 2 : 0)) assert(speed > Q12(4.7f));
            p->moveSpeed = speed;
            p->headingAngle = p->rotation.vy;
            func_8007C0D8(p, e, NULL);
            assert(collisionCalls == 1);
            assert(attempted.vx == 0);
            if (blocked) assert(p->position.vz == 0);
            else if (speed > 0) { assert(p->position.vz > before.position.vz); moved++; }
        }
        assert(blocked || moved);
        /* The owning FSM ends/interrupts the attack; locomotion must resume. */
        e->lowerBodyState = PlayerLowerBodyState_None;
        e->upperBodyState = PlayerUpperBodyState_None;
        g_SysWork.playerCombat.weaponAttack = NO_VALUE;
        controller.heldBtnFlags = 0;
        memset(keyboard, 0, sizeof(keyboard));
        keyboard[SDL_SCANCODE_W] = 1;
        TestShim(p, e);
        assert(D_800C4550 == Q12(1.5f));
        assert(e->lowerBodyState == PlayerLowerBodyState_WalkForward);
        scenarios++;
    }
    /* Other weapons and a ready katana must keep ordinary shim locomotion. */
    const int weapons[] = {NO_VALUE, EquippedWeaponId_KitchenKnife, EquippedWeaponId_SteelPipe,
                           EquippedWeaponId_Handgun, EquippedWeaponId_Shotgun, EquippedWeaponId_Katana};
    for (unsigned i = 0; i < sizeof(weapons) / sizeof(weapons[0]); i++) {
        memset(&g_SysWork, 0, sizeof(g_SysWork));
        memset(keyboard, 0, sizeof(keyboard));
        keyboard[SDL_SCANCODE_W] = 1;
        controller.heldBtnFlags = 0;
        g_SysWork.playerCombat.weaponAttack = weapons[i];
        e->lowerBodyState = weapons[i] == EquippedWeaponId_Katana ? PlayerLowerBodyState_Aim : PlayerLowerBodyState_Attack;
        TestShim(p, e);
        assert(D_800C4550 == Q12(1.5f));
        assert(p->model.anim.status == ANIM_STATUS(HarryAnim_WalkForward, false));
    }
    printf("PASS: %d katana scenarios; tap/hold/multitap variants x 30/60/120/240 fps x OTS/FPS x movement x aim-release x collision response; other weapons unchanged\n", scenarios);
    return 0;
}
