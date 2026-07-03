#include "global.h"
#include "assertf.h"
#include "battle_pyramid.h"
#include "battle_pyramid_bag.h"
#include "bg.h"
#include "constants/battle_pyramid.h"
#include "constants/characters.h"
#include "constants/field_weather.h"
#include "constants/unbound_start_menu.h"
#include "constants/unbound_start_menu.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "datetime.h"
#include "debug.h"
#include "decompress.h"
#include "even_sprite.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "field_screen_effect.h"
#include "field_weather.h"
#include "frontier_pass.h"
#include "gba/defines.h"
#include "gba/io_reg.h"
#include "gba/isagbprint.h"
#include "gba/macro.h"
#include "gba/types.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "item_menu.h"
#include "link.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "option_menu.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokedex.h"
#include "pokenav.h"
#include "rtc.h"
#include "safari_zone.h"
#include "save_dialog.h"
#include "script.h"
#include "sound.h"
#include "sprite.h"
#include "start_menu.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "trainer_card.h"
#include "trig.h"
#include "unbound_start_menu.h"
#include "util.h"
#include "window.h"
#include <string.h>
#include <sys/cdefs.h>

typedef bool8 (*Usm_MenuCB)(void);
typedef void (*Usm_ModeCB)(void);

#define USM_MAX_ICON_COUNT 6
#define USM_ICON_WIDTH 32
#define USM_BANNER_WIDTH 224
#define USM_ICON_YPOS 128

enum Usm_IconTiletags {
    USM_TILETAG_POKEDEX = 0x1000,
    USM_TILETAG_PARTY,
    USM_TILETAG_BAG,
    USM_TILETAG_POKENAV,
    USM_TILETAG_TRAINER,
    USM_TILETAG_SAVE,
    USM_TILETAG_OPTIONS,
    USM_TILETAG_RETIRE,
    USM_TILETAG_DEBUG,
    USM_TILETAG_HAND,
    USM_TILETAG_ARROW,
};

enum Usm_Paltags {
    USM_PALTAG_ICON = 0x1000 | BLEND_IMMUNE_FLAG,
};

enum Usm_Activation {
    USM_INACTIVE,
    USM_ACTIVE,
};


enum Usm_Mode
{
    USM_MODE_NORMAL,
    USM_MODE_MOVE,
    USM_MODE_SELECT,
};

enum Usm_Windows {
    USM_WIN_NAME,
    USM_WIN_CLOCK,
    USM_WIN_HINTS,
    USM_WIN_INFO,
    USM_WIN_COUNT,
};
struct Usm_VisibleIcons {
    u8 iconIndex[USM_MAX_ICON_COUNT];
    u8 count;
};

struct Usm_MoveState
{
    u8 grabIndex;
    u8 handSpriteId;
};

struct Usm_State {
    MainCallback savedCb;
    u8 loadState;
    u8 selectedVisibleIdx;
    u8 windowCount;
    u8 frameCounter;
    u8 dpadHeldFrames;
    u8 mainTaskId;
    u8 itemOffset;
    u8 items[USM_ICO_COUNT];
    u8 itemCount;
    enum Usm_Mode mode;
    struct Usm_MoveState move;
    struct Usm_VisibleIcons visible;
};

struct Usm_Memory {
    struct Usm_State state;
    u8 spriteIds[USM_MAX_ICON_COUNT];
    u8 windowIds[USM_WIN_COUNT];
    u8 leftArrowId;
    u8 rightArrowId;
};

struct Usm_MenuItem {
    enum Usm_Icons iconId;
    const struct SpriteTemplate* template;
    const struct CompressedSpriteSheet* sheet;
    const u8* label;
    bool8 shouldFade;
    Usm_MenuCB callback;
};

// Graphics
static const u32 sPokedexIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/pokedex.4bpp.smol");
static const u32 sPartyIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/party.4bpp.smol");
static const u32 sBagIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/bag.4bpp.smol");
static const u32 sPokenavIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/pokenav.4bpp.smol");
static const u32 sTrainerIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/trainer.4bpp.smol");
static const u32 sSaveIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/save.4bpp.smol");
static const u32 sOptionsIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/options.4bpp.smol");
static const u32 sDebugIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/debug.4bpp.smol");
static const u32 sRetireIconGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/retire.4bpp.smol");

static const u32 sUsmHandGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/hand.4bpp.smol");
static const u32 sUsmArrowGfx[] = INCBIN_U32("graphics/unbound_start_menu/sprites/arrow.4bpp.smol");

static const u8 sUsmStepGfx[] = INCBIN_U8("graphics/unbound_start_menu/step.4bpp");
static const u8 sUsmBallGfx[] = INCBIN_U8("graphics/unbound_start_menu/ball.4bpp");

static const u16 sIconPal[] = INCBIN_U16("graphics/unbound_start_menu/sprites/icons.gbapal");

static const u32 sUsmBgTiles[] = INCBIN_U32("graphics/unbound_start_menu/bg/tiles.4bpp.smol");
static const u32 sUsmBgTilemap[] = INCBIN_U32("graphics/unbound_start_menu/bg/map.bin.smolTM");
static const u16 sUsmBgPalette[] = INCBIN_U16("graphics/unbound_start_menu/bg/14.gbapal");

enum FontColor {
    FONT_WHITE,
    FONT_BLACK,
};

static const struct WindowTemplate sUsmWindowTemplates[] = {
    [USM_WIN_NAME] =
        {.bg = 0, .tilemapLeft = 0, .tilemapTop = 11, .width = 7, .height = 2, .paletteNum = 14, .baseBlock = 48},
    [USM_WIN_CLOCK] =
        {.bg = 0, .tilemapLeft = 19, .tilemapTop = 11, .width = 11, .height = 2, .paletteNum = 14, .baseBlock = 62},
    [USM_WIN_HINTS] =
        {.bg = 0, .tilemapLeft = 0, .tilemapTop = 18, .width = 8, .height = 2, .paletteNum = 14, .baseBlock = 84},
    [USM_WIN_INFO] =
        {.bg = 0, .tilemapLeft = 24, .tilemapTop = 18, .width = 6, .height = 2, .paletteNum = 14, .baseBlock = 124},
    DUMMY_WIN_TEMPLATE};


static const u8 sUsmWinFontColors[][3] = {
    [FONT_BLACK] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY},
    [FONT_WHITE] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY},
};

static const struct OamData sIconOam = {
    .x = 0,
    .y = 0,
    .objMode = ST_OAM_OBJ_NORMAL,
    .affineMode = ST_OAM_AFFINE_OFF,
    .affineParam = 0,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .matrixNum = 0,
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
};

static const union AnimCmd sAnim_Icon_Frame1[] = {ANIMCMD_FRAME(0, 30), ANIMCMD_END};

static const union AnimCmd sAnim_Icon_Frame2[] = {ANIMCMD_FRAME(16, 30), ANIMCMD_END};

static const union AnimCmd* const sIconAnimTable[] = {
    sAnim_Icon_Frame1,
    sAnim_Icon_Frame2,
};

static const union AffineAnimCmd sAffineAnim_RotateAndScale[] = {
    AFFINEANIMCMD_FRAME(10, 10, 0, 5),
    AFFINEANIMCMD_FRAME(-5, -5, 0, 10),
    AFFINEANIMCMD_FRAME(0, 0, 1, 4),

    AFFINEANIMCMD_FRAME(0, 0, -1, 4),
    AFFINEANIMCMD_FRAME(0, 0, 0, 2),
    AFFINEANIMCMD_FRAME(0, 0, -1, 4),
    AFFINEANIMCMD_FRAME(0, 0, 0, 2),
    AFFINEANIMCMD_FRAME(0, 0, -1, 4),

    AFFINEANIMCMD_FRAME(0, 0, 1, 4),
    AFFINEANIMCMD_FRAME(0, 0, 0, 2),
    AFFINEANIMCMD_FRAME(0, 0, 1, 4),
    AFFINEANIMCMD_FRAME(0, 0, 0, 2),
    AFFINEANIMCMD_FRAME(0, 0, 1, 4),

    AFFINEANIMCMD_JUMP(3),
};

static const union AffineAnimCmd* const sIconAffineAnimTable[] = {
    sAffineAnim_RotateAndScale
};

#define ICON_TEMPLATE(_icon, name)                                    \
    static const struct SpriteTemplate sSpriteTemplate_##name = {     \
        .tileTag = USM_TILETAG_##_icon,                               \
        .paletteTag = USM_PALTAG_ICON,                                \
        .oam = &sIconOam,                                             \
        .anims = sIconAnimTable,                                      \
        .affineAnims = sIconAffineAnimTable,                          \
        .callback = SpriteCallbackDummy,                              \
    };                                                                \
    static const struct CompressedSpriteSheet sSpriteSheet_##name = { \
        .data = s##name##IconGfx, .size = 0x400, .tag = USM_TILETAG_##_icon};

ICON_TEMPLATE(POKEDEX, Pokedex)
ICON_TEMPLATE(PARTY, Party)
ICON_TEMPLATE(BAG, Bag)
ICON_TEMPLATE(POKENAV, Pokenav)
ICON_TEMPLATE(TRAINER, Trainer)
ICON_TEMPLATE(SAVE, Save)
ICON_TEMPLATE(OPTIONS, Options)
ICON_TEMPLATE(DEBUG, Debug)
ICON_TEMPLATE(RETIRE, Retire)

static const struct SpritePalette sSpritePalette_Icons = {.data = sIconPal, .tag = USM_PALTAG_ICON};

// Static Variables
static EWRAM_DATA struct Usm_Memory* sUsmMemory;
static EWRAM_DATA struct Usm_State* sUsmState;
static EWRAM_DATA u8 sUsmSavedIcon = 0;
static EWRAM_DATA u8 sUsmSavedOffset = 0;

// Tasks
static void Task_UsmMain(u8 taskId);
static void Task_UsmFadeAndRunCallback(u8 taskId);
static void Task_UsmRunCallbackNoFade(u8 taskId);

// Static Functions
static void Usm_LoadBgGfx(void);
static void Usm_CreateIcons(s16 x, s16 y);
static void Usm_LoadIconGfx(void);
static void Usm_ShowSafariText(void);
static enum Usm_Icons Usm_GetSelectedIconId(void);
static struct Sprite* Usm_GetSelectedSprite(void);
static void Usm_BuildVisibleList(void);
static void Usm_SetupWindows();
static bool32 Usm_IsWindowVisible(enum Usm_Windows win);
static struct WindowTemplate Usm_GetDynamicWinTemplate(enum Usm_Windows);
static u8 Usm_GetWindowBaseColor(u8 winId);
static void Usm_PrintText(u8 winId, u8 fontId, s16 x, s16 y, const u8* color, const u8* str);
static void Usm_PrintIconLabel(void);
static void Usm_PrintClockText();
static void Usm_PrintButtonHints();
static void Usm_AnimateSelectedIcon(void);
static struct Sprite* Usm_GetIconSprite(u8 iconId);
static void Usm_ExitStartMenu(void);
static void Usm_SwitchSelectedIcon(enum Usm_Icons iconId);
static void Usm_HandleDPadInput();
static enum Usm_Icons Usm_GetNextIcon(s8 change);
static void BuildDateTimeString(u8* buf);
static void Usm_BuildMenuItems(void);
static void Usm_AddMenuItem(enum Usm_Icons icon);
static u32 Usm_CreateHandSprite(s16 x, s16 y);
static u32 Usm_CreateArrowSprite(s16 x, s16 y, bool32 flip);
static void Usm_SwapIconPos(u8 grabIndex, u8 targetIndex);
static void Usm_RedrawIcons();
static void Usm_DestroyVisibleIcons(void);
static void Usm_SetIconFrame(u8 visibleIndex, enum Usm_Activation activation);
static void Usm_StartIconAffineAnim(u8 visibleIndex);
static void Usm_StopIconAffineAnim(u8 visibleIndex);
static void Usm_StopIconAnim(u8 visibleIndex);
static void Usm_SaveItems(void);
static bool32 Usm_IsItemAvailable(enum Usm_Icons item);
static bool32 IsPlayerInBattlePyramid(void);
static void Usm_CreateScrollingArrows(void);
static bool32 Usm_IsFlashObscured(void);

// Menu Callbacks
static bool8 StartMenuPokedexCallback(void);
static bool8 StartMenuPokemonCallback(void);
static bool8 StartMenuBagCallback(void);
static bool8 StartMenuPokeNavCallback(void);
static bool8 StartMenuPlayerNameCallback(void);
static bool8 StartMenuSaveCallback(void);
static bool8 StartMenuOptionCallback(void);
static bool8 StartMenuExitCallback(void);
static bool8 StartMenuSafariZoneRetireCallback(void);
static bool8 StartMenuLinkModePlayerNameCallback(void);
static bool8 StartMenuBattlePyramidRetireCallback(void);
static bool8 StartMenuBattlePyramidBagCallback(void);
static bool8 StartMenuDebugCallback(void);
static bool8 StartMenuDexNavCallback(void);

static void Usm_HandleMainInput(void);
static void Usm_HandleMoveInput(void);
static void Usm_HandleSelection(void);

static Usm_ModeCB sUsmModeCallbacks[] = {
    [USM_MODE_NORMAL] = Usm_HandleMainInput,
    [USM_MODE_MOVE]   = Usm_HandleMoveInput,
    [USM_MODE_SELECT]   = Usm_HandleSelection,
};

static const struct Usm_MenuItem sUsmMenuItems[USM_ICO_COUNT] = {
    [USM_ICO_POKEDEX] =
        {
            .iconId = USM_ICO_POKEDEX,
            .template = &sSpriteTemplate_Pokedex,
            .sheet = &sSpriteSheet_Pokedex,
            .label = COMPOUND_STRING("Pokédex"),
            .shouldFade = TRUE,
            .callback = StartMenuPokedexCallback,
        },
    [USM_ICO_PARTY] =
        {
            .iconId = USM_ICO_PARTY,
            .template = &sSpriteTemplate_Party,
            .sheet = &sSpriteSheet_Party,
            .label = COMPOUND_STRING("Party"),
            .shouldFade = TRUE,
            .callback = StartMenuPokemonCallback,
        },
    [USM_ICO_BAG] =
        {
            .iconId = USM_ICO_BAG,
            .template = &sSpriteTemplate_Bag,
            .sheet = &sSpriteSheet_Bag,
            .label = COMPOUND_STRING("Bag"),
            .shouldFade = TRUE,
            .callback = StartMenuBagCallback,
        },
    [USM_ICO_POKENAV] =
        {
            .iconId = USM_ICO_POKENAV,
            .template = &sSpriteTemplate_Pokenav,
            .sheet = &sSpriteSheet_Pokenav,
            .label = COMPOUND_STRING("PokéNav"),
            .shouldFade = TRUE,
            .callback = StartMenuPokeNavCallback,
        },
    [USM_ICO_TRAINER] =
        {
            .iconId = USM_ICO_TRAINER,
            .template = &sSpriteTemplate_Trainer,
            .sheet = &sSpriteSheet_Trainer,
            .label = COMPOUND_STRING("Trainer"),
            .shouldFade = TRUE,
            .callback = StartMenuPlayerNameCallback,
        },
    [USM_ICO_SAVE] =
        {
            .iconId = USM_ICO_SAVE,
            .template = &sSpriteTemplate_Save,
            .sheet = &sSpriteSheet_Save,
            .label = COMPOUND_STRING("Save"),
            .shouldFade = FALSE,
            .callback = StartMenuSaveCallback,
        },
    [USM_ICO_OPTIONS] =
        {
            .iconId = USM_ICO_OPTIONS,
            .template = &sSpriteTemplate_Options,
            .sheet = &sSpriteSheet_Options,
            .label = COMPOUND_STRING("Options"),
            .shouldFade = TRUE,
            .callback = StartMenuOptionCallback,
        },
    [USM_ICO_DEBUG] =
        {
            .iconId = USM_ICO_DEBUG,
            .template = &sSpriteTemplate_Debug,
            .sheet = &sSpriteSheet_Debug,
            .label = COMPOUND_STRING("Debug"),
            .shouldFade = FALSE,
            .callback = StartMenuDebugCallback,
        },
    [USM_ICO_SAFARI_RETIRE] =
        {
            .iconId = USM_ICO_SAFARI_RETIRE,
            .template = &sSpriteTemplate_Retire,
            .sheet = &sSpriteSheet_Retire,
            .label = COMPOUND_STRING("Retire"),
            .shouldFade = FALSE,
            .callback = StartMenuSafariZoneRetireCallback,
        },
    [USM_ICO_FRONTIER_RETIRE] =
        {
            .iconId = USM_ICO_SAVE,
            .template = &sSpriteTemplate_Save,
            .sheet = &sSpriteSheet_Save,
            .label = COMPOUND_STRING("Retire"),
            .shouldFade = FALSE,
            .callback = StartMenuBattlePyramidRetireCallback,
        },
};

bool8 StartMenuPokedexCallback(void)
{
    if (!gPaletteFade.active)
    {
        IncrementGameStat(GAME_STAT_CHECKED_POKEDEX);
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_OpenPokedex);

        return TRUE;
    }

    return FALSE;
}

static bool8 StartMenuPokemonCallback(void)
{
    if (!gPaletteFade.active)
    {
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_PartyMenuFromStartMenu); // Display party menu

        return TRUE;
    }

    return FALSE;
}

static bool8 StartMenuBagCallback(void)
{
    if (IsPlayerInBattlePyramid())
    {
       return StartMenuBattlePyramidBagCallback();
    }
    else if (!gPaletteFade.active)
    {
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_BagMenuFromStartMenu); // Display bag menu

        return TRUE;
    }

    return FALSE;
}

static bool8 StartMenuPokeNavCallback(void)
{
    if (!gPaletteFade.active)
    {
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_InitPokeNav);  // Display PokéNav

        return TRUE;
    }

    return FALSE;
}

static bool8 StartMenuPlayerNameCallback(void)
{
    if (IsOverworldLinkActive()) {
        return StartMenuLinkModePlayerNameCallback();
    }
    else if (!gPaletteFade.active)
    {
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();

        if (IsOverworldLinkActive() || InUnionRoom())
            ShowPlayerTrainerCard (CB2_ReturnToFieldWithOpenMenu); // Display trainer card
        else if (FlagGet(FLAG_SYS_FRONTIER_PASS))
            ShowFrontierPass(CB2_ReturnToFieldWithOpenMenu); // Display frontier pass
        else
            ShowPlayerTrainerCard(CB2_ReturnToFieldWithOpenMenu); // Display trainer card

        return TRUE;
    }
    return FALSE;
}
static bool8 StartMenuSaveCallback(void)
{
    sUsmSavedIcon = 0;
    sUsmSavedOffset = 0;
    SaveDialog_InitSave();
    LockPlayerFieldControls();
    FreezeObjectEvents();
    CreateTask(Task_SaveDialogHandleSave, 0);
    return FALSE;
}

static bool8 StartMenuOptionCallback(void)
{
    if (!gPaletteFade.active)
    {
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_InitOptionMenu); // Display option menu
        gMain.savedCallback = CB2_ReturnToFieldWithOpenMenu;

        return TRUE;
    }

    return FALSE;
}
static bool8 UNUSED StartMenuExitCallback(void)
{
    Usm_ExitStartMenu();
    UnlockPlayerFieldControls();
    UnfreezeObjectEvents();
    return TRUE;
}

extern const u8 SafariZone_EventScript_RetirePrompt[];
static bool8 StartMenuSafariZoneRetireCallback(void)
{
    if (!gPaletteFade.active)
    {
        ScriptContext_SetupScript(SafariZone_EventScript_RetirePrompt);
    }
    return FALSE;
}

static bool8 StartMenuLinkModePlayerNameCallback(void)
{
    if (!gPaletteFade.active)
    {
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        ShowTrainerCardInLink(gLocalLinkPlayerId, CB2_ReturnToFieldWithOpenMenu);

        return TRUE;
    }

    return FALSE;
}

static bool8 StartMenuBattlePyramidRetireCallback(void)
{
    sUsmSavedIcon = 0;
    sUsmSavedOffset = 0;
    SaveDialog_InitBattlePyramidRetire();
    LockPlayerFieldControls();
    FreezeObjectEvents();
    CreateTask(Task_SaveDialogHandleBattlePyramidRetire, 0);
    return FALSE;
}

static bool8 StartMenuBattlePyramidBagCallback(void)
{
    if (!gPaletteFade.active)
    {
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_PyramidBagMenuFromStartMenu);

        return TRUE;
    }

    return FALSE;
}

static bool8 StartMenuDebugCallback(void)
{
    sUsmSavedOffset = 0;
    sUsmSavedIcon = 0;
    Debug_ShowMainMenu();

return TRUE;
}

static bool8 UNUSED StartMenuDexNavCallback(void)
{
    return FALSE;
}


static void Usm_SpriteCallbackArrow(struct Sprite *sprite)
{
    s32 maxOffset = SubtractClamped(0, USM_ICO_COUNT, sUsmState->itemCount, USM_MAX_ICON_COUNT);
    bool32 show;

    if (sprite->hFlip)
        show = (sUsmState->itemOffset > 0);
    else
        show = (sUsmState->itemOffset < maxOffset);

    u32 phase = (sUsmState->frameCounter * 5) % 256;
    s32 dis = Sin(phase, 2);
    s32 dir = sprite->hFlip ? -1 : 1;

    sprite->x2 = dis * dir;
    sprite->invisible = !show;
}



void Usm_InitStartMenu(void)
{
    if (!IsOverworldLinkActive()) {
        FreezeObjectEvents();
        PlayerFreeze();
        StopPlayerAvatar();
    }
    LockPlayerFieldControls();

    sUsmMemory = AllocZeroed(sizeof(struct Usm_Memory));

    if (sUsmMemory == NULL) {
        SetMainCallback2(CB2_ReturnToField);
        return;
    }

    if (Usm_IsFlashObscured())
    {
        SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
        SetGpuRegBits(REG_OFFSET_WINOUT, WINOUT_WINOBJ_OBJ);
    }

    sUsmState = &sUsmMemory->state;

    sUsmState->itemOffset = sUsmSavedOffset;
    sUsmState->selectedVisibleIdx = sUsmSavedIcon;
    sUsmSavedOffset = 0;
    sUsmSavedIcon = 0;
    Usm_BuildMenuItems();

    Usm_LoadBgGfx();
    Usm_SetupWindows();
    Usm_BuildVisibleList();
    Usm_PrintClockText();
    Usm_PrintButtonHints();
    Usm_PrintIconLabel();
    Usm_LoadIconGfx();
    Usm_LoadIconPalette();
    sUsmState->mode = USM_MODE_NORMAL;
    Usm_CreateIcons(0, USM_ICON_YPOS);
    Usm_AnimateSelectedIcon();
    Usm_CreateScrollingArrows();
    Usm_ShowSafariText();
    sUsmState->mainTaskId = CreateTask(Task_UsmMain, 1);
}

static void Usm_ShowSafariText(void)
{
    if (!GetSafariZoneFlag())
        return;

    u8 winId = sUsmMemory->windowIds[USM_WIN_INFO];
    FillWindowPixelBuffer(winId, PIXEL_FILL(Usm_GetWindowBaseColor(USM_WIN_INFO)));

    BlitBitmapToWindow(winId, sUsmStepGfx, 2, 5, 8, 8);
    ConvertIntToDecimalStringN(gStringVar1, gSafariZoneStepCounter, STR_CONV_MODE_LEFT_ALIGN, 3);
    Usm_PrintText(winId, FONT_SMALL, 10, 1, sUsmWinFontColors[FONT_WHITE], gStringVar1);

    u32 len = GetStringWidth(FONT_SMALL, gStringVar1, 0) + 12;

    BlitBitmapToWindow(winId, sUsmBallGfx, len, 5, 8, 8);
    ConvertIntToDecimalStringN(gStringVar2, gNumSafariBalls, STR_CONV_MODE_LEFT_ALIGN, 2);
    Usm_PrintText(winId, FONT_SMALL, len + 8, 1, sUsmWinFontColors[FONT_WHITE], gStringVar2);
    CopyWindowToVram(winId, COPYWIN_FULL);
}

static void Task_UsmMain(u8 taskId)
{
    if (JOY_HELD(DPAD_ANY))
        sUsmState->dpadHeldFrames++;
    else
        sUsmState->dpadHeldFrames = 0;

    sUsmState->frameCounter++;
    sUsmModeCallbacks[sUsmState->mode]();
}

static void Usm_CreateScrollingArrows(void)
{
    sUsmMemory->leftArrowId = Usm_CreateArrowSprite(12, USM_ICON_YPOS, TRUE);
    sUsmMemory->rightArrowId = Usm_CreateArrowSprite(DISPLAY_WIDTH - 12, USM_ICON_YPOS, FALSE);
}

static void Usm_PrintText(u8 winId, u8 fontId, s16 x, s16 y, const u8* color, const u8* str)
{
    AddTextPrinterParameterized4(winId, fontId, x, y, 0, 0, color, TEXT_SKIP_DRAW, str);
}

static void Usm_PrintIconLabel(void)
{
    u8 iconId = Usm_GetSelectedIconId();
    const u8* text = sUsmMenuItems[iconId].label;
    u8 winId = sUsmMemory->windowIds[USM_WIN_NAME];
    s16 x = GetStringCenterAlignXOffset(FONT_SMALL, text, GetWindowAttribute(winId, WINDOW_WIDTH) * 8);
    FillWindowPixelBuffer(winId, PIXEL_FILL(Usm_GetWindowBaseColor(USM_WIN_NAME)));
    Usm_PrintText(winId, FONT_SMALL, x, 0, sUsmWinFontColors[FONT_WHITE], text);
    CopyWindowToVram(winId, COPYWIN_GFX);
}

static void Usm_PrintClockText()
{
    u8 winId = sUsmMemory->windowIds[USM_WIN_CLOCK];
    BuildDateTimeString(gStringVar4);
    s16 x = GetStringCenterAlignXOffset(FONT_SMALL, gStringVar4, GetWindowAttribute(winId, WINDOW_WIDTH) * 8);
    FillWindowPixelBuffer(winId, PIXEL_FILL(Usm_GetWindowBaseColor(USM_WIN_CLOCK)));
    Usm_PrintText(sUsmMemory->windowIds[USM_WIN_CLOCK], FONT_SMALL, x, 0, sUsmWinFontColors[FONT_BLACK], gStringVar4);
    CopyWindowToVram(winId, COPYWIN_GFX);
}

static void Usm_PrintButtonHints()
{
    u8 winId = sUsmMemory->windowIds[USM_WIN_HINTS];
    const u8* text = COMPOUND_STRING("{SELECT_BUTTON} Move");
    s16 x = GetStringCenterAlignXOffset(FONT_SMALL_NARROWER, text, GetWindowAttribute(winId, WINDOW_WIDTH) * 8);
    FillWindowPixelBuffer(winId, PIXEL_FILL(Usm_GetWindowBaseColor(USM_WIN_HINTS)));
    Usm_PrintText(winId, FONT_SMALL_NARROWER, x, 0, sUsmWinFontColors[FONT_WHITE], text);
    CopyWindowToVram(winId, COPYWIN_GFX);
}

static void Usm_SetupWindows()
{
    DeactivateAllTextPrinters();

    for (u32 i = 0; i < USM_WIN_COUNT; i++) {
        if(!Usm_IsWindowVisible(i))
            continue;
        struct WindowTemplate templ = Usm_GetDynamicWinTemplate(i);
        u8 winId = AddWindow(&templ);
        FillWindowPixelBuffer(winId, PIXEL_FILL(Usm_GetWindowBaseColor(i)));
        sUsmMemory->windowIds[i] = winId;
        sUsmState->windowCount++;
        PutWindowTilemap(winId);
        CopyWindowToVram(winId, COPYWIN_GFX);
    }
}

static bool32 Usm_IsWindowVisible(enum Usm_Windows win)
{
    switch (win) {
     case USM_WIN_INFO: return GetSafariZoneFlag();
     default: return TRUE;
    }
}

static struct WindowTemplate Usm_GetDynamicWinTemplate(enum Usm_Windows win)
{
    struct WindowTemplate templ = sUsmWindowTemplates[win];

    switch (win) {
    case USM_WIN_INFO:
    case USM_WIN_HINTS:
        if (!GetSafariZoneFlag())
            templ.tilemapLeft = 11;
    default:
        return templ;
    }
}

static u8 Usm_GetWindowBaseColor(u8 winId)
{
    switch (winId) {
        case USM_WIN_CLOCK:
            return 15;
        case USM_WIN_INFO:
            return 10;
        case USM_WIN_NAME:
            return 11;
        case USM_WIN_HINTS:
            return GetSafariZoneFlag() ? 11 : 10;
        default:
            return 1;
    }
}

static void Usm_LoadBgGfx(void)
{
    u8* buffer = GetBgTilemapBuffer(0);
    CpuFastFill(0, buffer, BG_SCREEN_SIZE)
    LoadBgTilemap(0, 0, 0, 0);
    DecompressAndLoadBgGfxUsingHeap(0, sUsmBgTiles, 0, 0, 0);
    DecompressDataWithHeaderWram(sUsmBgTilemap, buffer);
    LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    LoadPalette(sUsmBgPalette, BG_PLTT_ID(14), PLTT_SIZE_4BPP);
    SetBgTilemapBuffer(0, buffer);
    ScheduleBgCopyTilemapToVram(0);
}


static const u8* const sMonthNames[13] = {
    [MONTH_JAN] = COMPOUND_STRING("Jan"), [MONTH_FEB] = COMPOUND_STRING("Feb"), [MONTH_MAR] = COMPOUND_STRING("Mar"),
    [MONTH_APR] = COMPOUND_STRING("Apr"), [MONTH_MAY] = COMPOUND_STRING("May"), [MONTH_JUN] = COMPOUND_STRING("Jun"),
    [MONTH_JUL] = COMPOUND_STRING("Jul"), [MONTH_AUG] = COMPOUND_STRING("Aug"), [MONTH_SEP] = COMPOUND_STRING("Sep"),
    [MONTH_OCT] = COMPOUND_STRING("Oct"), [MONTH_NOV] = COMPOUND_STRING("Nov"), [MONTH_DEC] = COMPOUND_STRING("Dec"),
};

static const u8* const sWeekdayNames[WEEKDAY_COUNT] = {
    [WEEKDAY_SUN] = COMPOUND_STRING("Sun"), [WEEKDAY_MON] = COMPOUND_STRING("Mon"),
    [WEEKDAY_TUE] = COMPOUND_STRING("Tue"), [WEEKDAY_WED] = COMPOUND_STRING("Wed"),
    [WEEKDAY_THU] = COMPOUND_STRING("Thu"), [WEEKDAY_FRI] = COMPOUND_STRING("Fri"),
    [WEEKDAY_SAT] = COMPOUND_STRING("Sat"),
};

static void BuildDateTimeString(u8* buf)
{
    u8 formattedBuffer[256];
    formattedBuffer[0] = EOS;
    u8* formattedBufferEnd = &formattedBuffer[0];

    struct DateTime dt;
    ConvertTimeToDateTime(&dt, &gLocalTime);

    u32 hour = (dt.hour + 11) % 12 + 1;
    const u8* am = COMPOUND_STRING("AM");
    const u8* pm = COMPOUND_STRING("PM");

    formattedBufferEnd = StringAppend(formattedBufferEnd, sWeekdayNames[dt.dayOfWeek]);
    formattedBufferEnd = StringAppend(formattedBufferEnd, COMPOUND_STRING(". "));
    formattedBufferEnd = ConvertIntToDecimalStringN(formattedBufferEnd, dt.day, STR_CONV_MODE_LEADING_ZEROS, 2);
    formattedBufferEnd = StringAppend(formattedBufferEnd, COMPOUND_STRING(", "));
    formattedBufferEnd = ConvertIntToDecimalStringN(formattedBufferEnd, hour, STR_CONV_MODE_LEADING_ZEROS, 2);
    formattedBufferEnd = StringAppend(formattedBufferEnd, COMPOUND_STRING(":"));
    formattedBufferEnd = ConvertIntToDecimalStringN(formattedBufferEnd, dt.minute, STR_CONV_MODE_LEADING_ZEROS, 2);
    formattedBufferEnd = StringAppend(formattedBufferEnd, COMPOUND_STRING(" "));
    formattedBufferEnd = StringAppend(formattedBufferEnd, (dt.hour >= 12) ? pm : am);

    StringCopy(buf, formattedBuffer);
}

static void Usm_AddMenuItem(enum Usm_Icons icon)
{
    if (sUsmState->itemCount >= USM_ICO_COUNT)
        return;

    sUsmState->items[sUsmState->itemCount++] = icon;
}

static bool32 Usm_ListContains(enum Usm_Icons item, u8 *list, u8 count)
{
    for (u32 i = 0; i < count; i++)
        if (list[i] == item)
            return TRUE;
    return FALSE;
}

static bool32 UNUSED Usm_ShouldPrepend(enum Usm_Icons item)
{
    switch (item)
    {
        case USM_ICO_POKEDEX:
        case USM_ICO_PARTY:
        case USM_ICO_POKENAV:
            return TRUE;
        default:
            return FALSE;
    }
}

static const enum Usm_Icons sUsmDefaultItems[] = {
    USM_ICO_DEBUG,   USM_ICO_POKEDEX, USM_ICO_PARTY, USM_ICO_BAG,
    USM_ICO_POKENAV, USM_ICO_TRAINER, USM_ICO_SAVE,  USM_ICO_OPTIONS,
    USM_ICO_SAFARI_RETIRE, USM_ICO_FRONTIER_RETIRE
};

static u32 Usm_GetDefaultIndex(enum Usm_Icons item)
{
    for (u32 i = 0; i < ARRAY_COUNT(sUsmDefaultItems); i++)
    {
        if (sUsmDefaultItems[i] == item)
            return i;
    }

    errorf("Unknown default menu item: %d", item);
    return ARRAY_COUNT(sUsmDefaultItems);
}

static void Usm_InsertSavedItem(enum Usm_Icons item)
{
    struct Usm_SavedItems *saved = &gSaveBlock3Ptr->usmSaved;
    u32 insertIndex = saved->count;
    u32 defaultIndex = Usm_GetDefaultIndex(item);

    u32 arrayCount = ARRAY_COUNT(saved->items);

    assertf(saved->count < arrayCount, "Saved menu full (count=%u, capacity=%u)", saved->count, arrayCount);

    for (u32 i = 0; i < saved->count; i++)
    {
        if (Usm_GetDefaultIndex(saved->items[i]) > defaultIndex)
        {
            insertIndex = i;
            break;
        }
    }

    u8* to = &saved->items[insertIndex + 1];
    u8* from = &saved->items[insertIndex];
    u32 size = (saved->count - insertIndex) * sizeof(saved->items[0]);

    memmove(to, from, size);

    saved->items[insertIndex] = item;
    saved->count++;
}

static void Usm_BuildMenuItems(void)
{
    struct Usm_SavedItems* saved = &gSaveBlock3Ptr->usmSaved;

    sUsmState->itemCount = 0;

    if (!saved->count)
    {
        for (u32 i = 0; i < ARRAY_COUNT(sUsmDefaultItems); i++)
        {
            enum Usm_Icons item = sUsmDefaultItems[i];

            if (!Usm_IsItemAvailable(item))
                continue;

            saved->items[saved->count++] = item;
        }
    }

    for (u32 i = 0; i < ARRAY_COUNT(sUsmDefaultItems); i++)
    {
        enum Usm_Icons item = sUsmDefaultItems[i];

        if (!Usm_IsItemAvailable(item))
            continue;

        if (Usm_ListContains(item, saved->items, saved->count))
            continue;

        Usm_InsertSavedItem(item);
    }

    for (u32 i = 0; i < saved->count; i++)
    {
        enum Usm_Icons item = saved->items[i];

        if (item >= USM_ICO_COUNT)
            continue;

        if (!Usm_IsItemAvailable(item))
            continue;

        Usm_AddMenuItem(item);
    }
}

static bool32 Usm_IsItemAvailable(enum Usm_Icons item)
{
    switch (item) {
        case USM_ICO_POKEDEX: return FlagGet(FLAG_SYS_POKEDEX_GET);
        case USM_ICO_PARTY: return FlagGet(FLAG_SYS_POKEMON_GET);
        case USM_ICO_POKENAV: return FlagGet(FLAG_SYS_POKENAV_GET);
        case USM_ICO_FRONTIER_RETIRE: return IsPlayerInBattlePyramid();
        case USM_ICO_SAVE: return !GetSafariZoneFlag() && !IsPlayerInBattlePyramid();
        case USM_ICO_SAFARI_RETIRE: return GetSafariZoneFlag();
        default: return TRUE;
    }

}

static void Usm_BuildVisibleList(void)
{
    u8 start = sUsmState->itemOffset;
    u8 end = start + USM_MAX_ICON_COUNT;

    if (end > sUsmState->itemCount)
        end = sUsmState->itemCount;

    sUsmState->visible.count = 0;

    for (u8 i = start; i < end; i++) {
        u8 itemId = sUsmState->items[i];
        sUsmState->visible.iconIndex[sUsmState->visible.count++] = itemId;
    }
}

void Usm_LoadIconPalette(void)
{
    LoadSpritePalette(&sSpritePalette_Icons);
    PreservePaletteInWeather(IndexOfSpritePaletteTag(USM_PALTAG_ICON) + 16);
}

static void Usm_CreateIcons(s16 x, s16 y)
{
    u8 count = sUsmState->visible.count;

    s16 startX = 24 + (USM_BANNER_WIDTH - (count * USM_ICON_WIDTH)) / 2;

    for (u32 i = 0; i < count; i++) {
        s16 posX = startX + i * USM_ICON_WIDTH;

        u8 iconId = sUsmState->visible.iconIndex[i];

        u8 id = CreateSprite(sUsmMenuItems[iconId].template, posX, y, 1);
        if (Usm_IsFlashObscured())
            gSprites[id].copyToObjWin = TRUE;
        sUsmMemory->spriteIds[i] = id;
    }
}

static void Usm_LoadIconGfx(void)
{
    for (u32 i = 0; i < USM_ICO_COUNT; i++) {
        LoadCompressedSpriteSheet(sUsmMenuItems[i].sheet);
    }
}

static enum Usm_Icons Usm_GetSelectedIconId(void)
{
    return sUsmState->visible.iconIndex[sUsmState->selectedVisibleIdx];
}

static struct Sprite* Usm_GetSelectedSprite(void)
{
    u8 selectedId = sUsmMemory->spriteIds[sUsmState->selectedVisibleIdx];
    struct Sprite* sprite = &gSprites[selectedId];
    return sprite;
}

static void Usm_AnimateSelectedIcon(void)
{
    for (u32 i = 0; i < sUsmState->visible.count; i++)
    {
        if (i != sUsmState->selectedVisibleIdx)
        {
            Usm_StopIconAnim(i);
        }
        else
        {
            Usm_SetIconFrame(i, USM_ACTIVE);

            if (sUsmState->mode == USM_MODE_NORMAL)
                Usm_StartIconAffineAnim(i);
        }
    }
}

static void Usm_StartIconAffineAnim(u8 visibleIndex)
{
    struct Sprite *sprite = Usm_GetIconSprite(visibleIndex);
    sprite->oam.affineMode = ST_OAM_AFFINE_NORMAL;
    u8 matrixNum = AllocOamMatrix();
    sprite->oam.matrixNum = matrixNum;
    StartSpriteAffineAnim(sprite, 0);
}

static void Usm_StopIconAffineAnim(u8 visibleIndex)
{
    struct Sprite* sprite = Usm_GetIconSprite(visibleIndex);
    FreeSpriteOamMatrix(sprite);
}

static void Usm_SetIconFrame(u8 visibleIndex, enum Usm_Activation activation)
{
    struct Sprite* sprite = Usm_GetIconSprite(visibleIndex);
    StartSpriteAnim(sprite, activation);
}

static void Usm_StopIconAnim(u8 visibleIndex)
{
    Usm_SetIconFrame(visibleIndex, USM_INACTIVE);
    Usm_StopIconAffineAnim(visibleIndex);
}

static struct Sprite* Usm_GetIconSprite(u8 iconId)
{
    struct Sprite* sprite = &gSprites[sUsmMemory->spriteIds[iconId]];
    return sprite;
}

static void Usm_ExitStartMenu(void)
{
    Usm_SaveItems();
    u8* buf = GetBgTilemapBuffer(0);

    Usm_DestroyVisibleIcons();

    for (u32 i = 0; i < USM_ICO_COUNT; i++) {
        FreeSpriteTilesByTag(sUsmMenuItems[i].template->tileTag);
    }

    for (u32 i = 0; i < sUsmState->windowCount; i++) {
        u8 winId = sUsmMemory->windowIds[i];
        FillWindowPixelBuffer(winId, PIXEL_FILL(0));
        ClearWindowTilemap(winId);
        CopyWindowToVram(winId, COPYWIN_FULL);
        RemoveWindow(winId);
    }
    FreeSpritePaletteByTag(USM_PALTAG_ICON);
    ResetPreservedPalettesInWeather();

    DestroySprite(&gSprites[sUsmMemory->leftArrowId]);
    DestroySprite(&gSprites[sUsmMemory->rightArrowId]);
    FreeSpriteTilesByTag(USM_TILETAG_ARROW);

    DestroyTask(FindTaskIdByFunc(Task_UsmMain));

    CpuFastFill(0, buf, BG_SCREEN_SIZE);
    CpuFastFill(0, (void*)BG_CHAR_ADDR(2), BG_CHAR_SIZE);
    ScheduleBgCopyTilemapToVram(0);

    TRY_FREE_AND_SET_NULL(sUsmMemory);
}

static void Usm_SaveItems(void)
{
    struct Usm_SavedItems* saved = &gSaveBlock3Ptr->usmSaved;

    u8 count = sUsmState->itemCount;

    if (count > USM_ICO_COUNT)
        count = USM_ICO_COUNT;

    saved->count = count;

    for (u8 i = 0; i < count; i++)
        saved->items[i] = sUsmState->items[i];
}

static void Usm_HandleMainInput(void)
{
    if (JOY_NEW(A_BUTTON))
    {
        u8 iconId = Usm_GetSelectedIconId();

        PlaySE(SE_SELECT);
        gMenuCallback = sUsmMenuItems[iconId].callback;
        sUsmSavedIcon = sUsmState->selectedVisibleIdx;
        sUsmSavedOffset = sUsmState->itemOffset;

        sUsmState->mode = USM_MODE_SELECT;
        return;
    }

    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_PC_OFF);
        Usm_ExitStartMenu();
        UnfreezeObjectEvents();
        UnlockPlayerFieldControls();
        return;
    }

    if (JOY_NEW(SELECT_BUTTON))
    {
        PlaySE(SE_SELECT);

        sUsmState->mode = USM_MODE_MOVE;
        sUsmState->move.grabIndex = sUsmState->itemOffset + sUsmState->selectedVisibleIdx;

        struct Sprite *sprite = Usm_GetSelectedSprite();
        Usm_StopIconAffineAnim(sUsmState->selectedVisibleIdx);

        sUsmState->move.handSpriteId =
            Usm_CreateHandSprite(sprite->x, sprite->y - 8);

        return;
    }

    Usm_HandleDPadInput();
}

static void Usm_HandleDPadInput()
{
    u8 input = 0;
    u8 curr = sUsmState->selectedVisibleIdx;
    u8 lastVisble = sUsmState->visible.count - 1;
    u8 last = sUsmState->itemCount - 1;

    input = JOY_NEW(DPAD_ANY);

    if (!input && (sUsmState->dpadHeldFrames % 8 == 1) && sUsmState->dpadHeldFrames > 30)
        input = JOY_HELD(DPAD_ANY);

    if (!input)
        return;

    PlaySE(SE_SELECT);

    if (input & (DPAD_DOWN | DPAD_RIGHT))
    {
        if (curr == lastVisble)
        {
            if (curr + sUsmState->itemOffset < last)
            {
                sUsmState->itemOffset++;
                Usm_RedrawIcons();
                Usm_PrintIconLabel();
            }
            return;
        }

        Usm_SwitchSelectedIcon(Usm_GetNextIcon(1));
    }

    else if (input & (DPAD_UP | DPAD_LEFT))
    {
        if (curr == 0)
        {
            if (sUsmState->itemOffset > 0)
            {
                sUsmState->itemOffset--;
                Usm_RedrawIcons();
                Usm_PrintIconLabel();
            }
            return;
        }

        Usm_SwitchSelectedIcon(Usm_GetNextIcon(-1));
    }
}

static void Task_UsmFadeAndRunCallback(u8 taskId)
{
    s16* tState = &gTasks[taskId].data[0];

    switch (*tState) {
        case 0:
            FadeScreen(FADE_TO_BLACK, 0);
            (*tState)++;
            break;
        case 1:
            if (!gPaletteFade.active) {
                Usm_ExitStartMenu();
                gMenuCallback();
                DestroyTask(taskId);
            }
            break;
    }
}

static void Task_UsmRunCallbackNoFade(u8 taskId)
{
    if (!gPaletteFade.active) {
        Usm_ExitStartMenu();
        gMenuCallback();
        DestroyTask(taskId);
    }
}

static void Usm_HandleSelection(void)
{
    u8 iconId = Usm_GetSelectedIconId();
    TaskFunc func = sUsmMenuItems[iconId].shouldFade
        ? Task_UsmFadeAndRunCallback
        : Task_UsmRunCallbackNoFade;

    gTasks[sUsmState->mainTaskId].func = func;
}

static void Usm_HandleMoveInput(void)
{
    if (JOY_NEW(B_BUTTON | SELECT_BUTTON))
    {
        DestroySprite(&gSprites[sUsmState->move.handSpriteId]);
        FreeSpriteTilesByTag(USM_TILETAG_HAND);

        sUsmState->mode = USM_MODE_NORMAL;

        Usm_RedrawIcons();
        return;
    }

    s8 dir = 0;
    u16 dpad = JOY_NEW(DPAD_ANY);

    if (dpad & (DPAD_UP | DPAD_LEFT))
        dir = -1;
    else if (dpad & (DPAD_DOWN | DPAD_RIGHT))
        dir = 1;

    if (dir != 0)
    {
        s16 targetIndex = sUsmState->move.grabIndex + dir;

        if (targetIndex >= 0 && targetIndex < sUsmState->itemCount)
        {
            u8 curr = sUsmState->selectedVisibleIdx;
            u8 lastVisible = sUsmState->visible.count - 1;

            if (dir > 0 && curr == lastVisible)
                sUsmState->itemOffset++;
            else if (dir < 0 && curr == 0)
                sUsmState->itemOffset--;

            Usm_SwapIconPos(sUsmState->move.grabIndex, targetIndex);
            sUsmState->move.grabIndex = targetIndex;
        }
    }

    struct Sprite *hand = &gSprites[sUsmState->move.handSpriteId];
    struct Sprite *target = Usm_GetSelectedSprite();

    hand->x = target->x;
    hand->y = target->y - 8;
}

static void Usm_SwapIconPos(u8 grabIndex, u8 targetIndex)
{
    Swap(sUsmState->items[grabIndex], sUsmState->items[targetIndex]);
    sUsmState->selectedVisibleIdx = targetIndex - sUsmState->itemOffset;
    Usm_RedrawIcons();
    Usm_PrintIconLabel();
}

static void Usm_RedrawIcons()
{
    Usm_BuildVisibleList();
    Usm_DestroyVisibleIcons();
    Usm_CreateIcons(0, USM_ICON_YPOS);
    Usm_AnimateSelectedIcon();
}

static void Usm_DestroyVisibleIcons(void)
{
    for (u32 i = 0; i < sUsmState->visible.count; i++) {
        struct Sprite* sprite = &gSprites[sUsmMemory->spriteIds[i]];
        FreeSpriteOamMatrix(sprite);
        DestroySprite(sprite);
    }
}

static enum Usm_Icons Usm_GetNextIcon(s8 change)
{
    u8 count = sUsmState->visible.count;
    s8 val = sUsmState->selectedVisibleIdx + change;
    if (val >= count)
    {
        val = 0;
    }
    else if (val < 0)
        val = count - 1;
    return val;
}

static void Usm_SwitchSelectedIcon(enum Usm_Icons iconId)
{
    sUsmState->selectedVisibleIdx = iconId;
    Usm_AnimateSelectedIcon();
    Usm_PrintIconLabel();
}


static u32 Usm_CreateArrowSprite(s16 x, s16 y, bool32 flip)
{
    u8 spriteId = Even_CreateSpriteParametrized(
        sUsmArrowGfx, USM_TILETAG_ARROW, sIconPal, USM_PALTAG_ICON,
        SPRITE_SIZE(32x32), SPRITE_SHAPE(32x32), x, y, 0, Usm_SpriteCallbackArrow,
        TRUE);
    gSprites[spriteId].oam.priority = 0;
    if (Usm_IsFlashObscured())
        gSprites[spriteId].copyToObjWin = TRUE;
    gSprites[spriteId].hFlip = flip;
    gSprites[spriteId].invisible = TRUE;
    return spriteId;
}

static u32 Usm_CreateHandSprite(s16 x, s16 y)
{
    u8 spriteId = Even_CreateSpriteParametrized(
        sUsmHandGfx, USM_TILETAG_HAND, sIconPal, USM_PALTAG_ICON,
        SPRITE_SIZE(32x32), SPRITE_SHAPE(32x32), x, y, 0, SpriteCallbackDummy,
        TRUE);
    gSprites[spriteId].oam.priority = 0;
    if (Usm_IsFlashObscured())
        gSprites[spriteId].copyToObjWin = TRUE;
    return spriteId;
}

static void Task_WaitForFadeShowUsm(u8 taskId)
{
    if (IsWeatherNotFadingIn()) {
        DestroyTask(taskId);
        Usm_InitStartMenu();
        FadeInFromBlack();
    }
}

void ReturnToFieldOpenUsm(void)
{
    CreateTask(Task_WaitForFadeShowUsm, 0x50);
    LockPlayerFieldControls();
}

bool8 FieldCB_ReturnToFieldUsm(void)
{
    ReturnToFieldOpenUsm();
    return TRUE;
}

static bool32 IsPlayerInBattlePyramid(void)
{
    return CurrentBattlePyramidLocation() != PYRAMID_LOCATION_NONE;
}

static bool32 Usm_IsFlashObscured(void)
{
    return IsPlayerInBattlePyramid() || GetFlashLevel();
}

