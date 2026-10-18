/*
 * Alfabet Śmierci for modern PCs: window, timing, input and presentation.
 *
 * The game logic runs on its own thread (scripts + native routines) and parks at every
 * vertical blank; the main thread advances it at 50 Hz (PAL) and shows the result with
 * integer nearest-neighbour scaling. The Amiga's 640x256 hires frame is shown as 640x512
 * (lines doubled), times the largest integer that fits.
 */
#include "amiga.h"
#include "app_paths.h"
#include "audio.h"
#include "natives.h"
#include "panels.h"
#include "script.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_NS (SDL_NS_PER_SECOND / 50)

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static uint32_t canvas[CANVAS_W * SCREEN_H];
static uint32_t game_rgb[SCREEN_W * SCREEN_H];

typedef struct {
    int scale;          /* integer scale of one hires pixel horizontally (vertically x2) */
    int ox, oy;         /* window position of canvas x=0 */
    bool panels;        /* side panels visible */
} Layout;

static Layout layout_for(int w, int h, bool want_panels)
{
    Layout l = {1, 0, 0, false};
    int s = 1;
    while (SCREEN_W * (s + 1) <= w && SCREEN_H * 2 * (s + 1) <= h) s++;
    l.scale = s;
    l.panels = want_panels && (w - SCREEN_W * s) / 2 >= PANEL_W * s;
    int cw = (l.panels ? CANVAS_W : SCREEN_W) * s;
    l.ox = (w - cw) / 2;
    l.oy = (h - SCREEN_H * 2 * s) / 2;
    return l;
}

static int game_thread(void *ud)
{
    (void)ud;
    VM vm;
    memset(&vm, 0, sizeof vm);
    /* like the floppy: the intro program first; it hands over to the game itself */
    int pc = SDL_getenv("ALF_SKIP_INTRO") ? -1 : script_routine_pc_by_name("intro");
    if (pc >= 0 && amiga_load_intro_program()) {
        vm_run(&vm, pc);
    } else {
        amiga_load_main();
        pc = script_routine_pc(MAIN_LOAD);
        if (pc < 0) { fprintf(stderr, "no boot routine\n"); return 1; }
        vm_run(&vm, pc);
    }
    fprintf(stderr, "game thread ended\n");
    for (;;) amiga_vbl();
}

/* Test helper: ALF_WARP=loc_XX jumps into that location once the game has booted;
 * ALF_ITEMS=7B3F2,7B400,... puts those item names (pointers) into the inventory. */
static void warp_check(uint32_t frame)
{
    static bool done;
    const char *w = SDL_getenv("ALF_WARP");
    if (done || !w || frame < 150) return;
    done = true;
    const char *items = SDL_getenv("ALF_ITEMS");
    if (items) {
        uint32_t slot = 0x7BBDA;
        for (const char *p = items; *p && slot < 0x7BE46;) {
            char *end;
            uint32_t a = (uint32_t)SDL_strtoul(p, &end, 16);
            if (end == p) break;
            wr32(slot, a);
            slot += 4;
            p = *end == ',' ? end + 1 : end;
        }
    }
    int pc = script_routine_pc_by_name(w);
    if (pc >= 0) vm_request_jump_pc(pc);
    else fprintf(stderr, "ALF_WARP: no routine %s\n", w);
}

/* ------------------------------------------------------------------ test driver
 * ALF_AUTOPLAY=file : lines "<frame> <action> [x y]" with actions move, lclick, lup, rclick,
 *                     rup, key, keyup, fire, fireup, qsave, qload (ALF_SAVE file),
 *                     mark <text> (log line with frame and location), shot <name> (saves the 640x256 game screen as BMP),
 *                     canvas <name>, pclick, oclick, dump <name>, quit.
 * ALF_FAST=1        : run frames as fast as possible (no 50 Hz pacing). */
typedef struct { uint32_t frame; char action[16]; int x, y; char arg[260]; } AutoEv;
static AutoEv auto_ev[16384];
static int auto_n, auto_i;
static bool auto_fast;


static void save_bmp(const char *name, const uint32_t *rgb)
{
    SDL_Surface *s = SDL_CreateSurfaceFrom(SCREEN_W, SCREEN_H, SDL_PIXELFORMAT_ARGB8888, (void *)rgb, SCREEN_W * 4);
    SDL_SaveBMP(s, name);
    SDL_DestroySurface(s);
}

/* apply events due at this frame; returns false on quit */
static uint32_t ticks;            /* 50 Hz ticks since start (test clock) */

static void save_canvas(const char *name, const uint32_t *c)
{
    SDL_Surface *s = SDL_CreateSurfaceFrom(CANVAS_W, SCREEN_H, SDL_PIXELFORMAT_ARGB8888, (void *)c, CANVAS_W * 4);
    SDL_SaveBMP(s, name);
    SDL_DestroySurface(s);
}

static struct { int w, h, hx, hy; bool shown; } g_cur_info;     /* the window cursor (tests) */

/* one test event; returns false on quit */
static bool apply_event(const AutoEv *e, AmigaInput *in, const uint32_t *rgb)
{
    if (!strcmp(e->action, "move")) { in->mouse_x = e->x; in->mouse_y = e->y; }
    else if (!strcmp(e->action, "lclick")) { in->mouse_x = e->x; in->mouse_y = e->y; in->lmb = true; }
    else if (!strcmp(e->action, "lup")) in->lmb = false;
    else if (!strcmp(e->action, "rclick")) in->rmb = true;
    else if (!strcmp(e->action, "rup")) in->rmb = false;
    else if (!strcmp(e->action, "key")) in->key = true;
    else if (!strcmp(e->action, "keyup")) in->key = false;
    else if (!strcmp(e->action, "fire")) in->fire = true;
    else if (!strcmp(e->action, "fireup")) in->fire = false;
    else if (!strcmp(e->action, "qsave")) panels_quick_save();
    else if (!strcmp(e->action, "qload")) panels_quick_load();
    else if (!strcmp(e->action, "mark"))
        fprintf(stderr, "mark %s frame %u loc $%02X at %s\n", e->arg, amiga_frame_count(), mem[0x7BBD4], vm_where_now());
    else if (!strcmp(e->action, "state")) {
        /* for tools/playthrough.py: where the game is and what it waits for */
        int mod, ord, row;
        uint64_t smp;
        int speed;
        double tempo;
        audio_position(&mod, &ord, &row, &smp);
        audio_tempo(&speed, &tempo);
        printf("state frame %u loc %02X where %s rwait %d table %X miny %u lmbpoll %u music %d %d %d %llu ms %llu"
               " speed %d tempo %.2f title %d cursor %d %dx%d hot %d,%d\n",
               amiga_frame_count(), mem[0x7BBD4], vm_where_now(), natives_waiting_rmb(), rd32(0x1CEC2),
               rd16(0x1CC04), amiga_frame_count() - amiga_last_lmb_poll(), mod, ord, row,
               (unsigned long long)smp, (unsigned long long)SDL_GetTicks(), speed, tempo, natives_title_shown(),
               g_cur_info.shown, g_cur_info.w, g_cur_info.h, g_cur_info.hx, g_cur_info.hy);
        fflush(stdout);
    }
    else if (!strcmp(e->action, "peek")) {              /* peek <hex address>: one byte */
        uint32_t a = (uint32_t)strtoul(e->arg, NULL, 16);
        printf("peek %X %u\n", a, rd8(a));
        fflush(stdout);
    }
    else if (!strcmp(e->action, "poke"))                /* poke <hex address> <byte> */
        wr8((uint32_t)strtoul(e->arg, NULL, 16), (uint32_t)e->y);
    else if (!strcmp(e->action, "inv")) {
        /* inventory: scroll offset (bytes) and the item name pointers, front first */
        printf("inv scroll %u items", rd32(0x7C0BE));
        for (uint32_t a = 0x7BBDA; a < 0x7BBDA + 155 * 4 && rd32(a); a += 4) printf(" %X", rd32(a));
        printf("\n");
        fflush(stdout);
    }
    else if (!strcmp(e->action, "shot")) save_bmp(e->arg, rgb);
    else if (!strcmp(e->action, "canvas")) { panels_compose(canvas, rgb, true); save_canvas(e->arg, canvas); }
    else if (!strcmp(e->action, "pclick")) panels_click(e->x, e->y, true, true);
    else if (!strcmp(e->action, "oclick")) panels_overlay_click();
    else if (!strcmp(e->action, "dump")) SDL_SaveFile(e->arg, mem, MEM_SIZE);
    else if (!strcmp(e->action, "quit")) return false;
    return true;
}

/* ALF_CONTROL=1: the same events read live from stdin, "<delay> <action> [x y]", where
 * delay counts frames after the previous command; "state" answers on stdout. */
static bool ctl_mode;
static SDL_Mutex *ctl_lock;
static AutoEv ctl_queue[1024];
static int ctl_head, ctl_tail;            /* ring buffer */
static uint32_t ctl_due;
static bool ctl_have;
static AutoEv ctl_cur;

static bool parse_event(const char *line, AutoEv *e)
{
    memset(e, 0, sizeof *e);
    int n = sscanf(line, "%u %15s %259s %d", &e->frame, e->action, e->arg, &e->y);
    if (n < 2 || line[0] == '#') return false;
    if (n >= 3) e->x = atoi(e->arg);
    return true;
}

static int ctl_reader(void *ud)
{
    (void)ud;
    char line[512];
    AutoEv e;
    while (fgets(line, sizeof line, stdin)) {
        if (!parse_event(line, &e)) continue;
        for (;;) {
            SDL_LockMutex(ctl_lock);
            bool full = (ctl_tail + 1) % 1024 == ctl_head;
            if (!full) { ctl_queue[ctl_tail] = e; ctl_tail = (ctl_tail + 1) % 1024; }
            SDL_UnlockMutex(ctl_lock);
            if (!full) break;
            SDL_Delay(1);
        }
    }
    return 0;
}

static bool control_step(AmigaInput *in, const uint32_t *rgb)
{
    for (;;) {
        if (!ctl_have) {
            SDL_LockMutex(ctl_lock);
            if (ctl_head != ctl_tail) {
                ctl_cur = ctl_queue[ctl_head];
                ctl_head = (ctl_head + 1) % 1024;
                ctl_have = true;
                ctl_due = ticks + ctl_cur.frame;
            }
            SDL_UnlockMutex(ctl_lock);
            if (!ctl_have) return true;
        }
        if (ticks < ctl_due) return true;
        ctl_have = false;
        if (!apply_event(&ctl_cur, in, rgb)) return false;
    }
}

/* ------------------------------------------------------------------ the pointer
 * The game's own pointer (its sprites) is the window's only cursor, a hardware cursor that
 * follows the real mouse everywhere, also over the side panels, with no frame of delay.
 * The sprites are drawn on their own layer, never into the picture; the cursor is rebuilt
 * at the window's scale whenever the game changes the pointer's shape. While the game's
 * pointer is off but the player can click (speech, end pages, panels, a room waiting for
 * input) the hand from the game data stands in. */
static uint32_t sprite_layer[SCREEN_W * SCREEN_H];
static uint32_t hand_img[32 * 16];
static SDL_Cursor *g_cursor;
static uint64_t g_cursor_key;
static bool g_cursor_shown;
static bool g_over_game = true;

static void cursor_show(bool show)
{
    if (show == g_cursor_shown) return;
    g_cursor_shown = show;
    if (show) SDL_ShowCursor(); else SDL_HideCursor();
}

static void cursor_update(int scale, bool can_click)
{
    int x0 = SCREEN_W, y0 = SCREEN_H, x1 = -1, y1 = -1;
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            if (sprite_layer[y * SCREEN_W + x]) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    const uint32_t *src;
    int w, h, stride, hx, hy;
    if (x1 >= 0) {                                   /* the game's pointer: hotspot = mouse */
        w = x1 - x0 + 1; h = y1 - y0 + 1;
        src = sprite_layer + y0 * SCREEN_W + x0;
        stride = SCREEN_W;
        hx = (int16_t)rd16(0x1CC12) - x0;
        hy = (int16_t)rd16(0x1CC14) / 2 - y0;
    } else if (can_click) {
        w = 32; h = 16; src = hand_img; stride = 32; hx = hy = 0;
    } else {
        cursor_show(false);
        g_cur_info.shown = false;
        return;
    }
    if (hx < 0) hx = 0;
    if (hx >= w) hx = w - 1;
    if (hy < 0) hy = 0;
    if (hy >= h) hy = h - 1;
    uint64_t key = 1469598103934665603ull ^ (uint64_t)(w | h << 8 | hx << 16 | hy << 24) ^ (uint64_t)scale << 40;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            key = (key ^ src[y * stride + x]) * 1099511628211ull;
    if (key != g_cursor_key) {
        SDL_Surface *s = SDL_CreateSurface(w * scale, h * 2 * scale, SDL_PIXELFORMAT_ARGB8888);
        if (!s) return;
        for (int y = 0; y < h * 2 * scale; y++) {
            uint32_t *row = (uint32_t *)((uint8_t *)s->pixels + y * s->pitch);
            for (int x = 0; x < w * scale; x++)
                row[x] = src[(y / (2 * scale)) * stride + x / scale];
        }
        SDL_Cursor *c = SDL_CreateColorCursor(s, hx * scale, hy * 2 * scale);
        SDL_DestroySurface(s);
        if (!c) return;
        SDL_SetCursor(c);
        if (g_cursor) SDL_DestroyCursor(g_cursor);
        g_cursor = c;
        g_cursor_key = key;
    }
    cursor_show(true);
    g_cur_info.w = w; g_cur_info.h = h; g_cur_info.hx = hx; g_cur_info.hy = hy; g_cur_info.shown = true;
}

static bool autoplay_step(AmigaInput *in, const uint32_t *rgb)
{
    if (ctl_mode) return control_step(in, rgb);
    while (auto_i < auto_n && auto_ev[auto_i].frame <= ticks)
        if (!apply_event(&auto_ev[auto_i++], in, rgb)) return false;
    return true;
}

static void autoplay_load(void)
{
    const char *path = SDL_getenv("ALF_AUTOPLAY");
    auto_fast = SDL_getenv("ALF_FAST") != NULL;
    if (SDL_getenv("ALF_CONTROL")) {
        ctl_mode = true;
        ctl_lock = SDL_CreateMutex();
        SDL_DetachThread(SDL_CreateThread(ctl_reader, "control", NULL));
        return;
    }
    if (!path) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (auto_n < 16384 && fgets(line, sizeof line, f))
        if (parse_event(line, &auto_ev[auto_n])) auto_n++;
    fclose(f);
}

static const char *base_dir(void)
{
    const char *b = SDL_GetBasePath();
    return b ? b : "./";
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (!app_paths_init()) {
        SDL_Quit();
        return 1;
    }
    char data[1024], scripts[1024];
    SDL_snprintf(data, sizeof data, "%sdata", base_dir());
    SDL_snprintf(scripts, sizeof scripts, "%sdata/scripts", base_dir());
    if (!amiga_load_disks(data)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Alfabet Śmierci",
                                 "Brak plików dyskietek (data/disk1.adf .. disk4.adf).", NULL);
        return 1;
    }
    if (!script_load_dir(scripts, natives, n_natives)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Alfabet Śmierci", "Błąd w skryptach (data/scripts).", NULL);
        return 1;
    }
    amiga_sync_init();
    amiga_set_fire_hook(natives_lmb_is_fire);
    autoplay_load();
    int trace = SDL_getenv("ALF_TRACE") ? SDL_atoi(SDL_getenv("ALF_TRACE")) : 0;
    if (SDL_getenv("ALF_TRACE") && trace <= 0) trace = 500;
    if (!audio_init())
        fprintf(stderr, "audio: %s\n", SDL_GetError());
    panels_init();
    video_hand_image(hand_img);           /* the main game is in memory now (before the intro) */

    /* window: the largest integer scale (with panels if they fit) inside 90% of the desktop;
     * the game starts fullscreen so the side panels are there from the first launch */
    SDL_Rect usable = {0, 0, 1920, 1080};
    SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable);
    int dw = usable.w, dh = usable.h, ws = 1;
    while (CANVAS_W * (ws + 1) <= dw * 9 / 10 && SCREEN_H * 2 * (ws + 1) <= dh * 9 / 10) ws++;
    int win_w = CANVAS_W * ws <= dw * 9 / 10 ? CANVAS_W * ws : SCREEN_W * ws;
    win = SDL_CreateWindow("Alfabet Śmierci  (port " PORT_VERSION ")", win_w, SCREEN_H * 2 * ws,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!SDL_getenv("ALF_WINDOWED")) panels_set_fullscreen(true);
    ren = SDL_CreateRenderer(win, NULL);
    SDL_SetRenderVSync(ren, 1);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, CANVAS_W, SCREEN_H);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    SDL_HideCursor();

    bool game_started = false;
    bool quit = false, want_panels = true;
    int applied_scale = -1;
    bool applied_fs = false;
    Uint64 last = SDL_GetTicksNS(), acc = 0;
    AmigaInput in = {.mouse_x = 320, .mouse_y = 200};
    while (!quit && !panels_quit_requested()) {
        if (!game_started) {
            SDL_CreateThread(game_thread, "game", NULL);
            amiga_wait_first_vbl();
            game_started = true;
        }
        /* window mode and scale chosen in the options panel */
        if (panels_fullscreen_wanted() != applied_fs) {
            applied_fs = panels_fullscreen_wanted();
            SDL_SetWindowFullscreen(win, applied_fs);
        }
        if (!applied_fs && panels_scale_choice() != applied_scale) {
            applied_scale = panels_scale_choice();
            if (applied_scale > 0)
                SDL_SetWindowSize(win, CANVAS_W * applied_scale, SCREEN_H * 2 * applied_scale);
        }
        int ww, wh;
        SDL_GetRenderOutputSize(ren, &ww, &wh);
        Layout lay = layout_for(ww, wh, want_panels);
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) quit = true;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                bool port_key = true;
                if (e.key.key == SDLK_F11 || (e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT)))
                    panels_set_fullscreen(!panels_fullscreen_wanted());
                else if (e.key.key == SDLK_TAB) want_panels = !want_panels;
                else port_key = false;
                if (!port_key) in.key = true;
                if (e.key.key == SDLK_SPACE) in.fire = true;
            }
            if (e.type == SDL_EVENT_KEY_UP) {
                in.key = false;
                if (e.key.key == SDLK_SPACE) in.fire = false;
            }
            /* joystick in port 2: the game reads its fire button in a few places
             * ("Wyzwoliłeś moc!!!"); any button of any connected pad counts */
            if (e.type == SDL_EVENT_JOYSTICK_ADDED) SDL_OpenJoystick(e.jdevice.which);
            if (e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN || e.type == SDL_EVENT_JOYSTICK_BUTTON_UP)
                in.fire = e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN;
            if (!auto_n && !ctl_mode && (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP)) {
                float mx, my;
                SDL_GetMouseState(&mx, &my);
                float d = SDL_GetWindowPixelDensity(win);
                mx *= d; my *= d;
                int cx = (int)((mx - lay.ox) / lay.scale);            /* canvas x (hires units) */
                int cy = (int)((my - lay.oy) / (lay.scale * 2));       /* canvas line */
                int gx = cx - (lay.panels ? PANEL_W : 0);
                bool in_game = gx >= 0 && gx < SCREEN_W && cy >= 0 && cy < SCREEN_H;
                if (e.type != SDL_EVENT_MOUSE_MOTION && !in_game) {
                    int px = cx < PANEL_W ? cx : cx - PANEL_W - SCREEN_W;
                    if (cx < PANEL_W) panels_click(px, cy, lay.panels, e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                }
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && in_game && panels_overlay_active()) {
                    panels_overlay_click();
                    continue;
                }
                in.mouse_x = gx;
                in.mouse_y = (int)((my - lay.oy) / lay.scale);      /* half-lines */
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    bool down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    if (e.button.button == SDL_BUTTON_LEFT) in.lmb = down && in_game;
                    if (e.button.button == SDL_BUTTON_RIGHT) in.rmb = down;
                    if (e.button.button == SDL_BUTTON_MIDDLE) in.fire = down;   /* joystick fire */
                    if (!down) { in.lmb = in.lmb && false; }
                }
                panels_hover(cx < PANEL_W ? cx : -1, cy, lay.panels);
                g_over_game = in_game;
            }
        }
        amiga_set_input(&in);

        Uint64 now = SDL_GetTicksNS();
        if (auto_fast) acc = FRAME_NS * 10;
        acc += now - last;
        last = now;
        if (!auto_fast && acc > FRAME_NS * 5) acc = FRAME_NS * 5;
        while (acc >= FRAME_NS) {
            if (game_started && !panels_overlay_active())
                if (!amiga_run_frame(2000))
                    fprintf(stderr, "frame %u: game thread busy at %s\n", amiga_frame_count(), vm_where_now());
            acc -= FRAME_NS;
            ticks++;
            if (game_started) warp_check(amiga_frame_count());
            if (trace && ticks % (uint32_t)trace == 0)
                fprintf(stderr, "tick %u frame %u loc $%02X at %s\n", ticks, amiga_frame_count(), mem[0x7BBD4], vm_where_now());
            if (auto_n || ctl_mode) {
                video_render(game_rgb, NULL);
                if (!autoplay_step(&in, game_rgb)) { quit = true; break; }
                amiga_set_input(&in);
            }
        }

        video_render(game_rgb, sprite_layer);
        float density = SDL_GetWindowPixelDensity(win);
        int cursor_scale = SDL_max(1, (int)(lay.scale / (density > 0.0f ? density : 1.0f) + 0.5f));
        cursor_update(cursor_scale, !game_started || !g_over_game || panels_overlay_active() || natives_waiting_click() ||
                                    amiga_frame_count() - amiga_last_lmb_poll() <= 2);
        int cw = lay.panels ? CANVAS_W : SCREEN_W;
        panels_compose(canvas, game_rgb, lay.panels);
        void *pixels; int pitch;
        SDL_LockTexture(tex, NULL, &pixels, &pitch);
        int x0 = lay.panels ? 0 : PANEL_W;
        for (int y = 0; y < SCREEN_H; y++)
            memcpy((uint8_t *)pixels + y * pitch, canvas + y * CANVAS_W + x0, cw * 4);
        SDL_UnlockTexture(tex);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_FRect srcr = {0, 0, (float)cw, (float)SCREEN_H};
        SDL_FRect dst = {(float)lay.ox, (float)lay.oy, (float)(cw * lay.scale), (float)(SCREEN_H * 2 * lay.scale)};
        SDL_RenderTexture(ren, tex, &srcr, &dst);
        SDL_RenderPresent(ren);
    }
    audio_music_stop();
    SDL_Quit();
    return 0;
}
