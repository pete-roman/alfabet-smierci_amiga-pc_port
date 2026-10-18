/*
 * The game's helper routines that are not lifted to scripts: they talk to the blitter,
 * the copper, the sprites, the disks or the music player. Each one is a direct
 * re-implementation of the original routine at the address in its comment and works on
 * the same memory: same buffers, same copper words, same sprite data.
 */
#include "natives.h"
#include "app_paths.h"

#include "amiga.h"
#include "audio.h"
#include "script.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* addresses used by the routines below */
#define FRAME_BUF     0x610FEu     /* decrunched scene frame */
#define FONT_PTR      0x610FAu     /* current font sheet */
#define TEXT_PLANES   0x7A904u     /* 7 bitplane pointers used by the text printers */
#define GLYPH_REPEAT  0x1CAF2u
#define SLOW_TEXT     0x1C67Cu
#define PANEL_BACKUP  0x71D04u     /* marble text panel, 8 planes x 56 lines */
#define PANEL_SCREEN  0x9A5B8u     /* line 200 of the screen */
#define SCREEN_PLANE0 0x96738u
#define PLANE_SIZE    0x5000u
#define PAL_CURRENT   0x1C278u
#define PAL_TARGET    0x1C478u
#define PAL_HALF_FLAG 0x1C04Eu     /* 0: fades work on colours 0-127, 1: on 128-255 */
#define COPPER_SPR0   0x5922Eu
#define COPPER_SPR4   0x5924Eu
#define NULL_SPRITE   0x1CC16u
#define CUR_SPR_A     0x1CBFCu
#define CUR_SPR_B     0x1CC00u
#define HAND_A        0x58E7Eu
#define HAND_B        0x58EC6u
#define HOTSPOTS_PTR  0x1CEC2u
#define UI_HOTSPOTS   0x1D716u
#define MOUSE_X       0x1CC12u
#define MOUSE_Y       0x1CC14u
#define WANTED_DISK   0x4EA36u

/* A fade is CPU-bound on the A1200 (no vbl wait inside its 255 steps). These are the steps
 * the original manages per 50 Hz frame, fitted to brightness curves captured from the
 * original running in WinUAE (A1200, 68020 at real speed): ~9 for fade-in, ~8 for fade-out. */
#define FADE_IN_STEPS_PER_FRAME  9
#define FADE_OUT_STEPS_PER_FRAME 8

static void vbl_n(int n)
{
    while (n-- > 0)
        amiga_vbl();
}

/* ------------------------------------------------------------------ text */

/* draw one 8x8 glyph into `planes` text planes at byte offset `ofs` ($1C730/$1C7AA) */
static void draw_glyph(uint8_t ch, uint32_t ofs, int planes)
{
    uint32_t glyph = rd32(FONT_PTR);
    uint8_t c = (uint8_t)(ch - 0x20);
    while (c >= 0x50) {
        glyph += 0x280;
        c -= 0x50;
    }
    glyph += c;
    int repeat = (int)(int16_t)rd16(GLYPH_REPEAT);
    for (int p = 0; p < planes; p++) {
        uint32_t dst = rd32(TEXT_PLANES + p * 4) + ofs;
        for (int r = 0; r <= repeat; r++) {
            uint32_t src = glyph;
            for (int line = 0; line < 8; line++) {
                wr8(dst, rd8(dst) | rd8(src));
                src += 0x50;
                dst += 0x50;
            }
        }
    }
}

static void slow_text_pause(void)
{
    /* $1C8F6: 51 waits for raster line $E0 = 51 frames */
    if (rd8(SLOW_TEXT))
        vbl_n(51);
}

/* $1C6C8: print the text at a2 into the marble panel (from line 203, column 2).
 * $0A = new line (10 lines lower), $B9 = white text, $B2 = grey text. */
static void n_print(VM *vm)
{
    uint32_t p = vm->a[2];
    uint32_t row = 0x3F70, col = 2;
    for (;;) {
        uint8_t c = (uint8_t)rd8(p++);
        if (!c) break;
        if (c == 0x0A) { row += 0x320; col = 2; continue; }
        if (c == 0xB9) { wr16(0x59AEC, 0xFFF); wr16(0x59AF4, 0xFFF); continue; }
        if (c == 0xB2) { wr16(0x59AEC, 0xCCC); wr16(0x59AF4, 0x000); continue; }
        if (rd8(SLOW_TEXT)) { slow_text_pause(); slow_text_pause(); }
        draw_glyph(c, row + col, 6);
        col++;
    }
}

/* $1C7DA: print at a2 in the scene area (from line 20, column 6), 7 planes.
 * $14 / $19 switch between the two font sheets. */
static void n_print_alt(VM *vm)
{
    uint32_t p = vm->a[2];
    uint32_t row = 0x640, col = 6;
    for (;;) {
        uint8_t c = (uint8_t)rd8(p++);
        if (!c) break;
        if (c == 0x0A) { row += 0x320; col = 6; continue; }
        if (c == 0x14) { wr32(FONT_PTR, 0x95FAC); continue; }
        if (c == 0x19) { wr32(FONT_PTR, 0x9672C); continue; }
        slow_text_pause();
        draw_glyph(c, row + col, 7);
        col++;
    }
}

/* $1CA3C: print a2 at byte offset d1 (inventory slots, verb bar) */
static void n_print_at(VM *vm)
{
    uint32_t p = vm->a[2], ofs = vm->d[1];
    for (;;) {
        uint8_t c = (uint8_t)rd8(p++);
        if (!c) break;
        draw_glyph(c, ofs, 6);
        ofs++;
    }
}

/* $1953C: restore the text panel from its backup (blit 8 planes, 56 lines) */
static void n_clear_text(VM *vm)
{
    (void)vm;
    for (int p = 0; p < 8; p++)
        blit_copy(PANEL_BACKUP + p * 0x1180, PANEL_SCREEN + p * PLANE_SIZE, 0xE28, 0, 0);
}

/* $195A6: save the text panel into its backup */
static void n_save_text_panel(VM *vm)
{
    (void)vm;
    for (int p = 0; p < 8; p++)
        blit_copy(PANEL_SCREEN + p * PLANE_SIZE, PANEL_BACKUP + p * 0x1180, 0xE28, 0, 0);
}

/* ------------------------------------------------------------------ waits */

static volatile bool g_waiting_rmb;
bool natives_waiting_rmb(void) { return g_waiting_rmb; }

/* The original moves speech on with the right mouse button only (POTGOR bit 10, $1C960,
 * $16956). Port: the left button and space / fire do it too, as players expect. A left
 * click or fire still held from the action that started the speech is let go first, so it
 * cannot skip the first line. */
static bool advance_held(void)
{
    const AmigaInput *in = amiga_input();
    return in->rmb || in->lmb || in->fire;
}

static void wait_rmb_press_release(int limit_frames)
{
    g_waiting_rmb = true;
    int n = 0;
    while ((amiga_input()->lmb || amiga_input()->fire) && !amiga_input()->rmb && n++ < limit_frames)
        amiga_vbl();
    while (!advance_held() && n++ < limit_frames)
        amiga_vbl();
    n = 0;
    while (advance_held() && n++ < limit_frames)
        amiga_vbl();
    g_waiting_rmb = false;
}

static void n_cursor_off(VM *vm);
static void n_cursor_hand(VM *vm);

/* $1C960: hide the pointer, wait for a right click (press + release). The game's pointer
 * stays off as in the original; the port shows its hand cursor while the game waits. */
static void n_wait_right_click(VM *vm)
{
    n_cursor_off(vm);
    wait_rmb_press_release(0x1F0 * 0x33);
}

/* $1C90A: the same wait, then the hand pointer */
static void n_wait_right_click_hand(VM *vm)
{
    n_wait_right_click(vm);
    n_cursor_hand(vm);
}

/* $16956: wait for a right click without touching the pointer */
static void n_wait_right_click_fast(VM *vm)
{
    (void)vm;
    wait_rmb_press_release(1 << 30);
}

static void n_delay_200(VM *vm) { (void)vm; vbl_n(0xC9); }         /* $1C9B2 */
static void n_delay_c9c6(VM *vm) { (void)vm; vbl_n(0x60 * 0x33); }  /* $1C9C6 */

/* $1C9E2: idle counter used by some locations as a timeout */
static void n_delay_c9e2(VM *vm)
{
    (void)vm;
    uint32_t v = rd32(0x1CA0C) + 1;
    if ((int32_t)v >= 0x3E8) {
        wr8(0x1CA10, 1);
        v = 0;
    }
    wr32(0x1CA0C, v);
}

/* ------------------------------------------------------------------ pointer */

static void copper_sprites(uint32_t base, uint32_t s0, uint32_t s1, uint32_t s2, uint32_t s3)
{
    uint32_t v[4] = {s0, s1, s2, s3};
    for (int i = 0; i < 4; i++) {
        wr16(base + 2 + i * 8, v[i] >> 16);
        wr16(base + 6 + i * 8, v[i]);
    }
}

/* $1CC2A */
static void n_cursor_off(VM *vm)
{
    (void)vm;
    copper_sprites(COPPER_SPR0, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE);
    copper_sprites(COPPER_SPR4, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE);
}

/* $1CC70: the hand pointer (sprites 0+1, attached) */
static void n_cursor_hand(VM *vm)
{
    (void)vm;
    wr32(CUR_SPR_A, HAND_A);
    wr32(CUR_SPR_B, HAND_B);
    copper_sprites(COPPER_SPR0, HAND_A, HAND_B, NULL_SPRITE, NULL_SPRITE);
    copper_sprites(COPPER_SPR4, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE);
}

/* $1CD2C, $1CD88, $1CDE4, $1CE3E (+$1CCCA): show another pointer at the hand's position.
 * The mouse keeps moving the hand sprites; the shown pair gets the position copied. */
static void show_pointer(uint32_t a, uint32_t b, bool also_clear_upper)
{
    wr32(a, rd32(HAND_A));
    wr32(b, rd32(HAND_B));
    wr32(CUR_SPR_A, HAND_A);
    wr32(CUR_SPR_B, HAND_B);
    copper_sprites(COPPER_SPR0, a, b, NULL_SPRITE, NULL_SPRITE);
    if (also_clear_upper)
        copper_sprites(COPPER_SPR4, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE);
}

/* $1CCCA: the "busy" pointer follows the mouse for 51 frames, then the hand comes back */
static void n_cursor_busy(VM *vm)
{
    wr32(CUR_SPR_A, 0x58F0E);
    wr32(CUR_SPR_B, 0x58F56);
    copper_sprites(COPPER_SPR0, 0x58F0E, 0x58F56, NULL_SPRITE, NULL_SPRITE);
    copper_sprites(COPPER_SPR4, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE, NULL_SPRITE);
    vbl_n(51);
    n_cursor_hand(vm);
}

static void n_cursor_anim_cd2c(VM *vm) { (void)vm; show_pointer(0x58F9E, 0x58FE6, false); }
static void n_cursor_anim_cd88(VM *vm) { (void)vm; show_pointer(0x5902E, 0x59076, false); }
static void n_cursor_exit_a(VM *vm)    { (void)vm; show_pointer(0x590BE, 0x59106, false); }
static void n_cursor_exit_b(VM *vm)    { (void)vm; show_pointer(0x5914E, 0x59196, false); }

/* ------------------------------------------------------------------ hotspots */

static int32_t hit(uint32_t table)
{
    int16_t x = (int16_t)rd16(MOUSE_X), y = (int16_t)rd16(MOUSE_Y);
    for (int32_t i = 1; rd16(table) != 0xBACA; i++, table += 8) {
        if (x < (int16_t)rd16(table) || x > (int16_t)rd16(table + 2)) continue;
        if (y < (int16_t)rd16(table + 4) || y > (int16_t)rd16(table + 6)) continue;
        return i;
    }
    return -1;
}

/* the hit tests run every frame while the game waits for the player (tests watch that) */
static void n_hit_test(VM *vm) { amiga_note_lmb_poll(); vm->d[2] = (uint32_t)hit(rd32(HOTSPOTS_PTR)); }  /* $1D7BA */
static void n_hit_test_ui(VM *vm) { amiga_note_lmb_poll(); vm->d[3] = (uint32_t)hit(UI_HOTSPOTS); }     /* $1D7E0 */

/* The original reads the joystick fire button (port 2) where the power is set free: on the
 * bridge (loc_2D), at the crossroads (loc_34) and in the forest (loc_35). Port: a left click
 * on an empty spot of the picture (no hotspot, not the panel) is fire there too. Elsewhere
 * fire stays on space, the middle button and a pad, because in loc_13 it does harm. */
bool natives_lmb_is_fire(void)
{
    uint8_t loc = (uint8_t)rd8(0x7BBD4);
    if (!amiga_main_loaded() || (loc != 0x2D && loc != 0x34 && loc != 0x35) || !amiga_input()->lmb)
        return false;
    return hit(rd32(HOTSPOTS_PTR)) == -1 && hit(UI_HOTSPOTS) == -1;
}

/* ------------------------------------------------------------------ graphics */

static void n_decrunch(VM *vm)   /* $4E5DC: CrunchMania a0 -> a1 */
{
    uint32_t src = vm->a[0], dst = vm->a[1];
    if (src >= MEM_SIZE || dst >= MEM_SIZE) return;
    if (rd32(src) != 0x43724D21) return;                 /* 'CrM!' */
    crunchmania_decrunch(mem + src, mem + dst, MEM_SIZE - dst);
}

/* $193C0: blit the 464x169 frame (7 planes) from $610FE to the screen at (16,16) */
static void n_show_frame(VM *vm)
{
    (void)vm;
    uint32_t src = FRAME_BUF, dst = SCREEN_PLANE0 + 16 * 80 + 2;
    for (int p = 0; p < 7; p++) {
        blit_copy(src, dst, (169 << 6) | 29, 0, 80 - 58);
        src += 58 * 169;
        dst += PLANE_SIZE;
    }
}

/* $19374: clear 448x60 at (16,16) in 7 planes (the "Włóż dysk" band) */
static void clear_disk_band(void)
{
    uint32_t dst = SCREEN_PLANE0 + 16 * 80 + 2;
    for (int p = 0; p < 7; p++, dst += PLANE_SIZE)
        for (int y = 0; y < 60; y++)
            for (int x = 0; x < 56; x++)
                wr8(dst + y * 80 + x, 0);
}

/* $19610: generic blit, parameters at $196A8.. (src, dst, mods, size, increments, planes-1) */
static void n_blit(VM *vm)
{
    (void)vm;
    uint32_t src = rd32(0x196A8), dst = rd32(0x196AC);
    uint32_t mods = rd32(0x196B0);
    uint16_t size = (uint16_t)rd16(0x196B4);
    uint32_t sinc = rd32(0x196B6), dinc = rd32(0x196BA);
    int planes = (int16_t)rd16(0x196BE);
    amiga_vbl();                 /* the original syncs to raster lines $B0 then $10 */
    for (int p = 0; p <= planes; p++) {
        blit_copy(src, dst, size, (int16_t)(mods >> 16), (int16_t)mods);
        src += sinc;
        dst += dinc;
    }
}

/* $1B7A4: write the screen's bitplane pointers into the copper list */
static void n_set_bitplanes(VM *vm)
{
    (void)vm;
    uint32_t cop = rd32(0x1B7D6), base = rd32(0x1B7DE), size = rd32(0x1B7D2);
    int n = (int)rd32(0x1B7DA);
    for (int i = 0; i <= n; i++, cop += 8, base += size) {
        wr16(cop + 2, base >> 16);
        wr16(cop + 6, base);
    }
}

/* ------------------------------------------------------------------ palettes */

static uint32_t palette_copper_base(void)
{
    return rd8(PAL_HALF_FLAG) ? 0x596AEu : 0x5928Eu;
}

/* $1C180 / $1C1FC: write 128 x 0RGB from `src` into the copper (hi + lo nibble words) */
static void palette_to_copper(uint32_t src)
{
    uint32_t a1 = palette_copper_base() + 6;
    for (int bank = 0; bank < 4; bank++) {
        for (int i = 0; i < 32; i++, src += 4, a1 += 4) {
            uint8_t r = (uint8_t)rd8(src + 1), g = (uint8_t)rd8(src + 2), b = (uint8_t)rd8(src + 3);
            uint16_t hi = (uint16_t)((r & 0xF0) << 4 | (g & 0xF0) | (b >> 4));
            uint16_t lo = (uint16_t)((r & 0x0F) << 8 | (g & 0x0F) << 4 | (b & 0x0F));
            wr16(a1, hi);
            wr16(a1 + 0x84, lo);
        }
        a1 += 0x88;
    }
}

/* $19332: fade target = 128 colours at a0, current = black */
static void n_set_palette(VM *vm)
{
    uint32_t a0 = vm->a[0];
    for (int i = 0; i < 128; i++) {
        wr32(PAL_TARGET + i * 4, rd32(a0 + i * 4));
        wr32(PAL_CURRENT + i * 4, 0);
    }
}

/* $1C050: 255 steps, each component +1 towards the target, copper updated every step */
static void n_fade_in(VM *vm)
{
    (void)vm;
    for (int step = 0; step < 255; step++) {
        for (int i = 0; i < 128; i++)
            for (int c = 1; c < 4; c++) {
                uint32_t a = PAL_CURRENT + i * 4 + c;
                uint8_t cur = (uint8_t)rd8(a), tgt = (uint8_t)rd8(PAL_TARGET + i * 4 + c);
                if (cur != tgt) wr8(a, cur + 1);
            }
        palette_to_copper(PAL_CURRENT);
        if (step % FADE_IN_STEPS_PER_FRAME == FADE_IN_STEPS_PER_FRAME - 1)
            amiga_vbl();
    }
}

/* $1C0DA: 255 steps, each component -3 (never below 1-2: underflow is skipped) */
static void n_fade_out(VM *vm)
{
    (void)vm;
    for (int step = 0; step < 255; step++) {
        for (int i = 0; i < 128; i++)
            for (int c = 1; c < 4; c++) {
                uint32_t a = PAL_TARGET + i * 4 + c;
                uint8_t v = (uint8_t)rd8(a);
                if (!v) continue;
                uint8_t n = (uint8_t)(v - 3);
                if (n == 0xFF || n == 0xFE) continue;
                wr8(a, n);
            }
        palette_to_copper(PAL_TARGET);
        if (step % FADE_OUT_STEPS_PER_FRAME == FADE_OUT_STEPS_PER_FRAME - 1)
            amiga_vbl();
    }
}

/* ------------------------------------------------------------------ misc */

static void n_random(VM *vm)   /* $4E722: d0 = 1 .. d5-1 (hardware entropy in the original) */
{
    uint16_t range = (uint16_t)vm->d[5];
    uint16_t v = range > 1 ? (uint16_t)(1 + SDL_rand(range - 1)) : 1;
    vm->d[0] = (vm->d[0] & 0xFFFF0000u) | v;
}

static void n_seed_random(VM *vm) { (void)vm; wr16(0x4E82C, (uint16_t)(1 + SDL_rand(0xFFFE))); }   /* $4E6E4 */

static void n_music_start(VM *vm) { (void)vm; audio_music_start((int)rd32(0x7C0B6)); }   /* $1D884 */
static void n_music_stop(VM *vm) { (void)vm; audio_music_stop(); }                     /* $1D8A8 */

/* $4E82E: data disk change. The original shows "Włóż dysk N", polls the drive until the
 * disk's header matches, then "Wczytuję dane..." while it reads the tracks. Here the
 * prompt stays for two seconds and the disk goes in by itself. */
static void n_load_disk(VM *vm)
{
    static const struct { int disk; uint32_t prompt; int first_track, tracks; } spec[6] = {
        {0, 0, 0, 0},
        {2, 0x7FAF7, 1, 0x95}, {3, 0x7FB0B, 1, 0x9D}, {4, 0x7FB1F, 1, 0x51},
        {2, 0x7FAF7, 0x69, 10}, {4, 0x7FB1F, 0x24, 10},
    };
    int want = (int)rd8(WANTED_DISK);
    if (want < 1 || want > 5) return;
    bool swap = amiga_disk_loaded() != spec[want].disk;
    clear_disk_band();
    vm->a[2] = spec[want].prompt;
    n_print_alt(vm);
    vbl_n(swap ? 100 : 10);
    clear_disk_band();
    vm->a[2] = 0x7FB4F;                                   /* "Wczytuję dane..." */
    n_print_alt(vm);
    const uint8_t *adf = amiga_disk_image(spec[want].disk);
    uint32_t ofs = (uint32_t)spec[want].first_track * 0x1600u, len = (uint32_t)spec[want].tracks * 0x1600u;
    SDL_memcpy(mem + BLOB_BASE, adf + ofs, len);
    amiga_note_disk(spec[want].disk);
    vbl_n(25);
}

/* ------------------------------------------------------------------ save disk
 * The floppy version saves to its own "Dysk Stanu Gry": track 0 carries the 'STAN' header,
 * each slot is one track read into / written from $7C90C. The port keeps that disk as
 * saves/stan_gry.adf, created already formatted, so the game's own menu works unchanged. */
#define TRACK_BYTES 0x1600u

static FILE *save_disk(void)
{
    char path[1024];
    app_save_path(path, sizeof path, "stan_gry.adf");
    FILE *f = fopen(path, "r+b");
    if (!f) {
        f = fopen(path, "w+b");
        if (!f) return NULL;
        static uint8_t blank[901120];
        memcpy(blank, "STAN", 4);
        fwrite(blank, 1, sizeof blank, f);
    }
    return f;
}

static void track_io(bool write)
{
    uint32_t buf = rd32(0x4F81E);
    uint32_t track = rd16(0x4F822), count = rd16(0x4F824);
    FILE *f = save_disk();
    wr16(0x4F818, f ? 0 : 1);                /* status: 0 = ok */
    if (!f) return;
    fseek(f, (long)(track * TRACK_BYTES), SEEK_SET);
    if (buf + count * TRACK_BYTES <= MEM_SIZE) {
        if (write) fwrite(mem + buf, 1, count * TRACK_BYTES, f);
        else if (fread(mem + buf, 1, count * TRACK_BYTES, f) != count * TRACK_BYTES) wr16(0x4F818, 1);
    }
    fclose(f);
}

static void n_save_disk_read(VM *vm) { (void)vm; track_io(false); }    /* $4F232 */
static void n_save_disk_write(VM *vm) { (void)vm; track_io(true); }    /* $4F2B8 */
static void n_drive_on(VM *vm) { (void)vm; }                            /* $4F026: select drive, motor on */
static void n_drive_off(VM *vm) { (void)vm; }                           /* $4F044: motor off */

/* ------------------------------------------------------------------ intro program */

static bool g_title_shown;
bool natives_title_shown(void) { return g_title_shown; }

#define INTRO_LOGO_FILE  0x76DF8   /* first picture of the intro (Seven Stars logo) */
#define INTRO_TITLE_FILE 0x809DC   /* the title picture */
#define INTRO_TITLE_LABEL "IL031BA" /* intro $31BA: start of the title section */
static bool g_restart_at_title;

/* $42C2 in the intro: same CrunchMania decruncher. The title picture is the file at intro
 * $809DC; while it is on screen the presentation layer adds the port line.
 * After the ending the intro runs again from the title: its setup (music, copper,
 * interrupts) runs as usual, then the logo pictures are skipped. */
static void n_intro_decrunch(VM *vm)
{
    if (g_restart_at_title && vm->a[0] == INTRO_LOGO_FILE) {
        g_restart_at_title = false;
        int pc = script_label_pc(INTRO_TITLE_LABEL);
        if (pc >= 0) {
            vm->pc = pc;
            vm->jumped = true;
            return;
        }
        fprintf(stderr, "intro: no label %s, playing the whole intro\n", INTRO_TITLE_LABEL);
    }
    g_title_shown = vm->a[0] == INTRO_TITLE_FILE;
    n_decrunch(vm);
}

/* $13E8C `bra.s *`: where the original stops for good after the last ending page.
 * Port (Pete's decision): the last page stays on screen until a click or a key, then the
 * game goes back to the title screen, like switching the Amiga off and on again without
 * the logos. */
static bool any_input(void)
{
    const AmigaInput *in = amiga_input();
    return in->lmb || in->rmb || in->key || in->fire;
}

static volatile bool g_halt_wait;
bool natives_waiting_click(void) { return g_waiting_rmb || g_halt_wait; }

static void wait_any_click(void)
{
    g_halt_wait = true;
    while (any_input()) amiga_vbl();      /* the click that showed the page */
    while (!any_input()) amiga_vbl();
    while (any_input()) amiga_vbl();
    g_halt_wait = false;
}

/* The port's page after the original end credits, printed like them (game charset:
 * $F1 = ń, $F3 = ó), centred the way the original pages are. */
static const char *const port_end_page[] = {
    "", "", "",
    "Port by Pete Roman, for Seven Stars Multimedia",
    "and Maciej 'Maniak' Frontkiewicz, with love",
    "", "",
    "Piotr 'Pete' Roma\xF1" "czyk",
    "Autor portu z system\xF3w Amiga na PC",
};
#define END_PAGE_TEXT 0x7F02Fu    /* reuse the first thanks page's text buffer (555 bytes) */

static void show_port_end_page(VM *vm)
{
    uint32_t a = END_PAGE_TEXT;
    for (int i = 0; i < (int)(sizeof port_end_page / sizeof port_end_page[0]); i++) {
        int len = (int)strlen(port_end_page[i]);
        for (int k = 0; len && k < (52 - len) / 2; k++) wr8(a++, ' ');
        for (int k = 0; k < len; k++) wr8(a++, (uint8_t)port_end_page[i][k]);
        wr8(a++, '\n');
    }
    wr8(a, 0);
    vm_call_addr(vm, 0x1C016);            /* fade out, as between the original pages */
    vm_call_addr(vm, 0x14360);
    n_clear_text(vm);
    vm->a[2] = END_PAGE_TEXT;
    n_print_alt(vm);
    vm_call_addr(vm, 0x1C006);            /* fade in */
}

static void n_halt(VM *vm)
{
    wait_any_click();
    show_port_end_page(vm);
    wait_any_click();
    audio_music_stop();
    if (!amiga_load_intro_program()) {
        fprintf(stderr, "halt: intro program failed to load\n");
        for (;;) amiga_vbl();
    }
    amiga_reset_custom();
    memset(vm->d, 0, sizeof vm->d);
    memset(vm->a, 0, sizeof vm->a);
    g_restart_at_title = true;
    vm->pc = script_routine_pc_by_name("intro");
    vm->jumped = true;
}

static void n_intro_music_start(VM *vm) { (void)vm; audio_play_intro(); }   /* intro $43E0 */
static void n_intro_music_stop(VM *vm) { (void)vm; audio_stop_intro(); }    /* intro $4404 */

/* jmp $1FE4B0: the loader reads the main game from disk 1 sector 450 into $3000 and jumps
 * there. Memory is cleared and refilled from the ADF the same way. */
static void n_intro_end_load_game(VM *vm)
{
    g_title_shown = false;
    audio_stop_intro();
    amiga_load_main();
    amiga_reset_custom();
    memset(vm->d, 0, sizeof vm->d);
    memset(vm->a, 0, sizeof vm->a);
    vm->pc = script_routine_pc(0x3000);
    vm->jumped = true;
}

static void n_unimplemented(VM *vm)
{
    fprintf(stderr, "native routine not implemented yet (called from %s)\n", script_where(vm->pc));
}

const Native natives[] = {
    {"clear_text", 0x1953C, n_clear_text},
    {"native_195A6", 0x195A6, n_save_text_panel},
    {"print", 0x1C6C8, n_print},
    {"print_alt", 0x1C7DA, n_print_alt},
    {"print_at", 0x1CA3C, n_print_at},
    {"wait_right_click", 0x1C960, n_wait_right_click},
    {"wait_right_click_fast", 0x16956, n_wait_right_click_fast},
    {"wait_right_click_hand", 0x1C90A, n_wait_right_click_hand},
    {"delay_200", 0x1C9B2, n_delay_200},
    {"delay_c9c6", 0x1C9C6, n_delay_c9c6},
    {"delay_c9e2", 0x1C9E2, n_delay_c9e2},
    {"cursor_off", 0x1CC2A, n_cursor_off},
    {"cursor_hand", 0x1CC70, n_cursor_hand},
    {"cursor_busy", 0x1CCCA, n_cursor_busy},
    {"cursor_anim_cd2c", 0x1CD2C, n_cursor_anim_cd2c},
    {"cursor_anim_cd88", 0x1CD88, n_cursor_anim_cd88},
    {"cursor_exit_a", 0x1CDE4, n_cursor_exit_a},
    {"cursor_exit_b", 0x1CE3E, n_cursor_exit_b},
    {"hit_test", 0x1D7BA, n_hit_test},
    {"hit_test_ui", 0x1D7E0, n_hit_test_ui},
    {"decrunch", 0x4E5DC, n_decrunch},
    {"show_frame", 0x193C0, n_show_frame},
    {"blit", 0x19610, n_blit},
    {"native_1B7A4", 0x1B7A4, n_set_bitplanes},
    {"set_palette", 0x19332, n_set_palette},
    {"fade_in", 0x1C050, n_fade_in},
    {"fade_out", 0x1C0DA, n_fade_out},
    {"random", 0x4E722, n_random},
    {"native_4E6E4", 0x4E6E4, n_seed_random},
    {"music_start", 0x1D884, n_music_start},
    {"music_stop", 0x1D8A8, n_music_stop},
    {"load_disk", 0x4E82E, n_load_disk},
    {"halt", 0x13E8C, n_halt},
    {"native_4E92A", 0x4E92A, n_unimplemented},
    {"native_4E962", 0x4E962, n_unimplemented},
    {"native_4E98E", 0x4E98E, n_unimplemented},
    {"native_4E9AE", 0x4E9AE, n_unimplemented},
    {"intro_decrunch", 0, n_intro_decrunch},
    {"intro_music_start", 0, n_intro_music_start},
    {"intro_music_stop", 0, n_intro_music_stop},
    {"intro_end_load_game", 0, n_intro_end_load_game},
    {"native_4F026", 0x4F026, n_drive_on},
    {"native_4F044", 0x4F044, n_drive_off},
    {"native_4F232", 0x4F232, n_save_disk_read},
    {"native_4F2B8", 0x4F2B8, n_save_disk_write},
};
const int n_natives = (int)(sizeof natives / sizeof natives[0]);
