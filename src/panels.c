/*
 * Everything around the original screen: the side panels (options left, stone right),
 * the title screen with the port line, and the two credits screens.
 *
 * All of it is built at start-up from the game's own data in memory: the UI artwork
 * (screen at $96738, UI palette at $59AFA), the 8x8 font at $95FAC, the intro's title
 * picture and palette. Drawing happens in the Amiga's pixel grid (hires columns, PAL
 * lines), so the panels scale exactly like the game and never look sharper than it.
 */
#include "panels.h"
#include "app_paths.h"

#include "audio.h"
#include "natives.h"
#include "script.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UI_W 640
#define UI_H 256

static uint32_t ui_rgb[3][UI_W * UI_H];   /* UI artwork with the stone / blue / gold palette */
static uint32_t left_panel[SCREEN_H * PANEL_W], right_panel[SCREEN_H * PANEL_W];
static uint32_t title_rgb[SCREEN_W * SCREEN_H];
static bool have_title;
static uint8_t font[0x780];
static uint32_t stone[12];

static uint32_t text_c = 0xFFCCCCCC, text_hi = 0xFFFFFFFF;

enum { OV_NONE, OV_TITLE, OV_CREDITS, OV_TOOLS };
static int overlay = OV_NONE, page;
static bool quit_req;
static int hover_item = -1;
static char status_msg[32];
static int status_frames;

/* ------------------------------------------------------------------ text */

/* UTF-8 -> the game's Polish charset (lowercase from the game's text, uppercase = -$20) */
static int next_char(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    if (!*p) return 0;
    if (*p < 0x80) { (*s)++; return *p; }
    unsigned cp = 0;
    int len = (*p & 0xE0) == 0xC0 ? 2 : 3;
    cp = len == 2 ? (p[0] & 0x1F) << 6 | (p[1] & 0x3F) : (p[0] & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F);
    *s += len;
    static const struct { unsigned cp; int code; } tab[] = {
        {0x105, 0xE6}, {0x107, 0xE7}, {0x119, 0xEA}, {0x142, 0xEE}, {0x144, 0xF1}, {0xF3, 0xF3},
        {0x15B, 0xF5}, {0x17A, 0xFB}, {0x17C, 0xFE}, {0x104, 0xC6}, {0x106, 0xC7}, {0x118, 0xCA},
        {0x141, 0xCE}, {0x143, 0xD1}, {0xD3, 0xD3}, {0x15A, 0xD5}, {0x179, 0xDB}, {0x17B, 0xDE},
    };
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (tab[i].cp == cp) return tab[i].code;
    return '?';
}

static int text_len(const char *s) { int n = 0; while (next_char(&s)) n++; return n; }

/* draw into an ARGB buffer of width `w` (Amiga pixels); sx/sy scale the 8x8 glyphs */
static void draw_text_raw(uint32_t *buf, int w, int h, int x, int y, const char *s, uint32_t col, int sx, int sy, bool raw)
{
    int c;
    while ((c = raw ? (unsigned char)*s++ : next_char(&s)) != 0) {
        int i = c - 0x20;
        if (i < 0) { x += 8 * sx; continue; }
        int gx = (i % 80) * 8, gy = (i / 80) * 8;
        for (int r = 0; r < 8; r++) {
            uint8_t bits = font[(gy + r) * 80 + gx / 8];
            for (int k = 0; k < 8; k++)
                if (bits & (0x80 >> k))
                    for (int yy = 0; yy < sy; yy++)
                        for (int xx = 0; xx < sx; xx++) {
                            int px = x + k * sx + xx, py = y + r * sy + yy;
                            if (px >= 0 && px < w && py >= 0 && py < h) buf[py * w + px] = col;
                        }
        }
        /* the font marks Ś Ź Ć Ń with a notch that vanishes when enlarged: add an accent */
        if (sx > 1 && (c == 0xD5 || c == 0xDB || c == 0xC7 || c == 0xD1))
            for (int k = 0; k < 2 * sx; k++) {
                int px = x + 4 * sx + k;
                if (y - 2 >= 0 && px < w) buf[(y - 2) * w + px] = col;
                if (y - 3 >= 0 && px + sx < w) buf[(y - 3) * w + px + sx] = col;
            }
        x += 8 * sx;
    }
}

static void draw_text(uint32_t *buf, int w, int h, int x, int y, const char *s, uint32_t col, int sx, int sy)
{
    draw_text_raw(buf, w, h, x, y, s, col, sx, sy, false);
}

static void centred(uint32_t *buf, int w, int h, int cx, int y, const char *s, uint32_t col, int sx, int sy)
{
    draw_text(buf, w, h, cx - text_len(s) * 4 * sx, y, s, col, sx, sy);
}

/* ------------------------------------------------------------------ artwork */

static uint32_t argb(const uint8_t *p) { return 0xFF000000u | (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2]; }

static void decode_ui(void)
{
    static const uint32_t pal_addr[3] = {0x59AFA, 0x59CFA, 0x59EFA};   /* stone, blue, gold */
    for (int v = 0; v < 3; v++) {
        for (int y = 0; y < UI_H; y++)
            for (int x = 0; x < UI_W; x++) {
                int idx = 0;
                for (int p = 0; p < 8; p++)
                    idx |= ((mem[0x96738 + p * 0x5000 + y * 80 + x / 8] >> (7 - (x & 7))) & 1) << p;
                ui_rgb[v][y * UI_W + x] = idx >= 128 ? argb(mem + pal_addr[v] + (idx - 128) * 4 + 1) : 0xFF000000u;
            }
    }
    static const int ramp_idx[12] = {214, 242, 243, 245, 247, 248, 249, 250, 251, 252, 253, 254};
    for (int i = 0; i < 12; i++) stone[i] = argb(mem + 0x59AFA + (ramp_idx[i] - 128) * 4 + 1);
}

static void blit_rect(uint32_t *dst, int dw, int dx, int dy, const uint32_t *src, int sw, int sx, int sy, int w, int h)
{
    for (int y = 0; y < h; y++)
        memcpy(dst + (dy + y) * dw + dx, src + (sy + y) * sw + sx, w * 4);
}

/* The game's marble texture (bottom text panel) remapped by brightness rank onto the
 * middle of the stone ramp, stacked in 50-line slabs with a mortar line between. */
static void stone_texture(uint32_t *out, int w, int h, int offset)
{
    static uint8_t level[50 * 624];
    static bool made;
    if (!made) {
        int hist[1024] = {0}, total = 50 * 624;
        int lum[50 * 624];
        for (int y = 0; y < 50; y++)
            for (int x = 0; x < 624; x++) {
                uint32_t c = ui_rgb[0][(202 + y) * UI_W + 8 + x];
                int l = (int)((c >> 16) & 0xFF) + (int)((c >> 8) & 0xFF) + (int)(c & 0xFF);
                lum[y * 624 + x] = l;
                hist[l]++;
            }
        int cdf[1024], acc = 0;
        for (int i = 0; i < 1024; i++) { cdf[i] = acc; acc += hist[i]; }
        for (int i = 0; i < total; i++) level[i] = (uint8_t)((int64_t)cdf[lum[i]] * 6 / total);
        made = true;
    }
    for (int y = 0; y < h; y++) {
        int slab = y / 50, row = y % 50;
        int x0 = (offset + slab * 197) % (624 - w);
        for (int x = 0; x < w; x++) {
            int sx = slab & 1 ? x0 + w - 1 - x : x0 + x;
            out[y * w + x] = stone[4 + level[row * 624 + sx]];
        }
        if (row == 0 && y) { for (int x = 0; x < w; x++) { out[(y - 1) * w + x] = stone[2]; out[y * w + x] = stone[11]; } }
    }
}

static void bevel(uint32_t *buf, int bw, int x, int y, int w, int h, uint32_t light, uint32_t dark)
{
    for (int i = 0; i < w; i++) { buf[y * bw + x + i] = light; buf[(y + h - 1) * bw + x + i] = dark; }
    for (int i = 0; i < h; i++) { buf[(y + i) * bw + x] = light; buf[(y + i) * bw + x + w - 1] = dark; }
}

/* ------------------------------------------------------------------ side panels */

#define ITEM_X 26
#define ITEM_Y0 30
#define ITEM_DY 15
#define N_ITEMS 8
static const char *const item_label[N_ITEMS] = {"Muzyka", "Skala", "Pełny ekran", "Zapisz", "Wczytaj", "Autorzy",
                                                "Narzędzia", "Wyjście"};
static int volume_level = 4;     /* 0..6 */
static int g_scale_choice;       /* requested window scale (0 = auto) */
static bool g_fullscreen;

static void build_panels(void)
{
    stone_texture(left_panel, PANEL_W, SCREEN_H, 40);
    stone_texture(right_panel, PANEL_W, SCREEN_H, 300);
    bevel(left_panel, PANEL_W, 0, 0, PANEL_W, SCREEN_H, stone[11], 0xFF000000u | 0x4D4545);
    bevel(right_panel, PANEL_W, 0, 0, PANEL_W, SCREEN_H, stone[11], 0xFF000000u | 0x4D4545);
}

static void draw_left_panel(uint32_t *canvas)
{
    static uint32_t p[SCREEN_H * PANEL_W];
    memcpy(p, left_panel, sizeof p);
    /* header: the pink verb bar of the original panel, cut to slot width */
    blit_rect(p, PANEL_W, ITEM_X, 10, ui_rgb[0], UI_W, 518, 88, 108, 12);
    centred(p, PANEL_W, SCREEN_H, ITEM_X + 54, 12, "OPCJE", text_hi, 1, 1);
    for (int i = 0; i < N_ITEMS; i++) {
        int y = ITEM_Y0 + i * ITEM_DY;
        blit_rect(p, PANEL_W, ITEM_X, y, ui_rgb[0], UI_W, 519, 16, 108, 12);  /* inventory slot */
        uint32_t c = i == hover_item ? text_hi : text_c;
        char buf[32];
        const char *label = item_label[i];
        if (i == 1) {
            if (g_scale_choice) SDL_snprintf(buf, sizeof buf, "Skala  x%d", g_scale_choice);
            else SDL_snprintf(buf, sizeof buf, "Skala  auto");
            label = buf;
        }
        if (i == 2) label = g_fullscreen ? "Okno" : "Pełny ekran";
        if (i == 0) {
            draw_text(p, PANEL_W, SCREEN_H, ITEM_X + 6, y + 2, label, c, 1, 1);
            for (int k = 0; k < 6; k++) {
                uint32_t bc = k < volume_level ? text_hi : stone[2];
                for (int yy = 3; yy < 9; yy++)
                    for (int xx = 0; xx < 5; xx++) p[(y + yy) * PANEL_W + ITEM_X + 62 + k * 7 + xx] = bc;
            }
        } else {
            centred(p, PANEL_W, SCREEN_H, ITEM_X + 54, y + 2, label, c, 1, 1);
        }
    }
    if (status_frames > 0) {
        centred(p, PANEL_W, SCREEN_H, ITEM_X + 54, ITEM_Y0 + N_ITEMS * ITEM_DY + 6, status_msg, text_hi, 1, 1);
        status_frames--;
    }
    for (int y = 0; y < SCREEN_H; y++) memcpy(canvas + y * CANVAS_W, p + y * PANEL_W, PANEL_W * 4);
}

/* ------------------------------------------------------------------ credits */

static void rope_frame(uint32_t *s, const uint32_t *src)
{
    const int c = 20, h = 196, w = 640;
    for (int y = c; y < h - c; y++) {
        int sy = 40 + (y - c) % 100;
        memcpy(s + y * w, src + sy * UI_W, c * 4);
        memcpy(s + y * w + w - c, src + sy * UI_W + 496 - c, c * 4);
    }
    for (int y = 0; y < c; y++)
        for (int x = c; x < w - c; x++) {
            s[y * w + x] = src[y * UI_W + 240];
            s[(h - c + y) * w + x] = src[(197 - c + y) * UI_W + 240];
        }
    for (int y = 0; y < c; y++) {
        memcpy(s + y * w, src + y * UI_W, c * 4);
        memcpy(s + y * w + w - c, src + y * UI_W + 496 - c, c * 4);
        memcpy(s + (h - c + y) * w, src + (197 - c + y) * UI_W, c * 4);
        memcpy(s + (h - c + y) * w + w - c, src + (197 - c + y) * UI_W + 496 - c, c * 4);
    }
}

static void credits_base(uint32_t *s, int variant)
{
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) s[i] = 0xFF000000u;
    rope_frame(s, ui_rgb[variant]);
    blit_rect(s, SCREEN_W, 0, 198, ui_rgb[variant], UI_W, 0, 198, 640, 58);
}

/* the original's thanks pages, printed verbatim from the game's memory */
static const uint32_t thanks_pages[][2] = {
    {0x7F02F, 0}, {0x7F25B, 0}, {0x7F43E, 0}, {0x7F475, 0x7F6CB}, {0x7F6F5, 0}, {0x7F8F4, 0},
};
#define N_THANKS_PAGES (int)(sizeof thanks_pages / sizeof thanks_pages[0])

/* the port author's card, laid out like the original authors' personal pages */
static const char *const port_card[] = {
    "Piotr 'Pete' Romańczyk",
    "Autor portu z systemów Amiga na PC",
    "",
    "Osobiste podziękowania składam:",
    "Maciejowi 'Maniakowi' Frontkiewiczowi",
    "za kod, który po latach wciąż się broni,",
    "Ryszardowi 'Justusowi' Bagińskiemu",
    "za kadry, w których Dęby żyją do dziś,",
    "Michałowi 'Dixanowi' Prochownikowi",
    "za muzykę i historię, które zostają w głowie,",
    "całej ekipie Seven Stars Multimedia i Arte",
    "oraz mieszkańcom Żywca i Jeleśni,",
    "którzy dali tej grze swoje twarze i ulice.",
    "",
    "Dziękuję, że mogłem tu wrócić.",
};
#define N_CREDIT_PAGES (1 + N_THANKS_PAGES + 1)

static int draw_mem_block(uint32_t *s, uint32_t addr, int y, uint32_t col)
{
    char line[96];
    int n = 0;
    for (uint32_t a = addr;; a++) {
        uint8_t ch = mem[a];
        if (ch == '\n' || ch == 0) {
            line[n] = 0;
            draw_text_raw(s, SCREEN_W, SCREEN_H, 320 - 26 * 8, y, line, col, 1, 1, true);
            y += 9;
            n = 0;
            if (!ch) break;
            continue;
        }
        if (n < 95) line[n++] = (char)ch;
    }
    return y;
}

static void render_credits(uint32_t *s)
{
    credits_base(s, 0);
    uint32_t head = stone[10];
    if (page == 0) {
        centred(s, SCREEN_W, SCREEN_H, 320, 26, "ALFABET ŚMIERCI", text_hi, 2, 1);
        centred(s, SCREEN_W, SCREEN_H, 320, 40, "Arte  *  Seven Stars Multimedia  (C) 1995/1996", head, 1, 1);
        static const char *const rows[3][2] = {{"Programowanie", "Maciej 'Maniak' Frontkiewicz"},
                                               {"Grafika", "Ryszard 'Justus' Bagiński"},
                                               {"Muzyka i scenariusz", "Michał 'Dixan05' Prochownik"}};
        for (int i = 0; i < 3; i++) {
            draw_text(s, SCREEN_W, SCREEN_H, 316 - text_len(rows[i][0]) * 8, 60 + i * 12, rows[i][0], head, 1, 1);
            draw_text(s, SCREEN_W, SCREEN_H, 332, 60 + i * 12, rows[i][1], text_c, 1, 1);
        }
    } else if (page > N_THANKS_PAGES) {
        /* last page: the port line itself, then the port author's card */
        centred(s, SCREEN_W, SCREEN_H, 320, 16, "Port by Pete Roman, for Seven Stars Multimedia", text_hi, 1, 1);
        centred(s, SCREEN_W, SCREEN_H, 320, 25, "and Maciej 'Maniak' Frontkiewicz, with love", text_hi, 1, 1);
        for (int i = 0; i < (int)(sizeof port_card / sizeof port_card[0]); i++)
            centred(s, SCREEN_W, SCREEN_H, 320, 41 + i * 9, port_card[i], i < 2 ? text_hi : text_c, 1, 1);
    } else {
        int y = draw_mem_block(s, thanks_pages[page - 1][0], 22, text_c);
        if (thanks_pages[page - 1][1]) draw_mem_block(s, thanks_pages[page - 1][1], y, text_c);
    }
    char nav[32];
    SDL_snprintf(nav, sizeof nav, "%d/%d   kliknij: dalej", page + 1, N_CREDIT_PAGES);
    centred(s, SCREEN_W, SCREEN_H, 320, 208, "Port by Pete Roman, for Seven Stars Multimedia", text_hi, 1, 1);
    centred(s, SCREEN_W, SCREEN_H, 320, 220, "and Maciej 'Maniak' Frontkiewicz, with love", text_hi, 1, 1);
    centred(s, SCREEN_W, SCREEN_H, 320, 238, nav, text_c, 1, 1);
}

static const char *const tools[][3] = {
    {"SDL3", "Sam Lantinga i in.", "zlib"},
    {"libopenmpt", "OpenMPT Project Developers", "BSD-3"},
    {"miniz, minimp3", "R. Geldreich; lieff (w libopenmpt)", "MIT/CC0"},
    {"stb_vorbis", "Sean Barrett (w libopenmpt)", "MIT/PD"},
    {"The Player 6.0A", "Jarno Paananen (Guru)", "freeware"},
    {"ProWizard", "Sylvain 'Asle' Chipaux", "MIT"},
    {"libxmp", "C. Matsuoka, H. Carraro Jr i in.", "MIT"},
    {"ancient", "Teemu Suutari", "BSD-2"},
    {"CrunchMania", "Thomas Schwarz", "freeware"},
    {"WinUAE", "Toni Wilen i in. (UAE: B. Schmidt)", "GPL-2.0"},
    {"AROS ROM", "The AROS Development Team", "APL 1.1"},
    {"Capstone", "Nguyen Anh Quynh i in.", "BSD-3"},
    {"amitools", "Christian Vogelgsang", "GPL-2.0+"},
    {"Python", "Python Software Foundation", "PSF-2.0"},
    {"Pillow", "Alex Clark i in.", "MIT-CMU"},
    {"NumPy", "NumPy Developers", "BSD-3"},
    {"w64devkit / GCC", "C. Wellons; GNU Project; mingw-w64", "Unlicense/GPL"},
    {"CMake", "Kitware i in.", "BSD-3"},
    {"Git", "L. Torvalds, J. Hamano i in.", "GPL-2.0"},
    {"PowerShell", "Microsoft", "komponent Windows"},
    {"Claude Opus 5.5", "Anthropic", "komercyjna"},
    {"Claude Code", "Anthropic", "komercyjna"},
};
#define N_TOOLS (int)(sizeof tools / sizeof tools[0])
#define TOOLS_PER_PAGE 12
#define N_TOOL_PAGES ((N_TOOLS + TOOLS_PER_PAGE - 1) / TOOLS_PER_PAGE)

static void render_tools(uint32_t *s)
{
    credits_base(s, 1);
    uint32_t head = 0xFFC8D0F0;
    centred(s, SCREEN_W, SCREEN_H, 320, 26, "NARZĘDZIA I BIBLIOTEKI", text_hi, 2, 1);
    centred(s, SCREEN_W, SCREEN_H, 320, 40, "użyte do zbudowania i uruchomienia portu", head, 1, 1);
    for (int i = 0; i < TOOLS_PER_PAGE; i++) {
        int k = page * TOOLS_PER_PAGE + i;
        if (k >= N_TOOLS) break;
        int y = 56 + i * 10;
        draw_text(s, SCREEN_W, SCREEN_H, 32, y, tools[k][0], text_hi, 1, 1);
        draw_text(s, SCREEN_W, SCREEN_H, 176, y, tools[k][1], text_c, 1, 1);
        draw_text(s, SCREEN_W, SCREEN_H, 608 - text_len(tools[k][2]) * 8, y, tools[k][2], head, 1, 1);
    }
    if (page == N_TOOL_PAGES - 1) {
        centred(s, SCREEN_W, SCREEN_H, 320, 160, "Dostęp do narzędzi możliwy dzięki współpracy", text_hi, 1, 1);
        centred(s, SCREEN_W, SCREEN_H, 320, 170, "z ReadyM (ReadyCode Limited).", text_hi, 1, 1);
    }
    centred(s, SCREEN_W, SCREEN_H, 320, 212, "Pełne teksty licencji: THIRD_PARTY_LICENSES.txt", text_hi, 1, 1);
    char nav[32];
    SDL_snprintf(nav, sizeof nav, "%d/%d   kliknij: dalej", page + 1, N_TOOL_PAGES);
    centred(s, SCREEN_W, SCREEN_H, 320, 226, nav, text_c, 1, 1);
}

/* ------------------------------------------------------------------ title */

static void build_title(void)
{
    uint32_t n;
    uint8_t *intro = amiga_load_intro(&n);
    if (!intro || n != 576608) return;
    uint8_t *pic = malloc(163840);
    if (crunchmania_decrunch(intro + 0x7D9DC, pic, 163840) == 163840) {
        const uint8_t *pal = intro + 0x73250;          /* 256 x 0RGB, verified against WinUAE */
        for (int y = 0; y < SCREEN_H; y++)
            for (int x = 0; x < SCREEN_W; x++) {
                int idx = 0;
                for (int p = 0; p < 8; p++)
                    idx |= ((pic[p * 20480 + y * 80 + x / 8] >> (7 - (x & 7))) & 1) << p;
                title_rgb[y * SCREEN_W + x] = argb(pal + idx * 4 + 1);
            }
        centred(title_rgb, SCREEN_W, SCREEN_H, 360, 234, "Port by Pete Roman, for Seven Stars Multimedia", 0xFFB6B6B6, 1, 1);
        centred(title_rgb, SCREEN_W, SCREEN_H, 360, 244, "and Maciej 'Maniak' Frontkiewicz, with love", 0xFFB6B6B6, 1, 1);
        have_title = true;
    }
    audio_set_intro_module(intro + (0x5D9A - 0x3000));
    free(pic);
    free(intro);
}

/* ------------------------------------------------------------------ save / load */

#define STATE_ADDR 0x7BBD4u
#define STATE_SIZE 0x69Cu
static volatile int g_req;            /* 1 = save, 2 = load (handled on the game thread) */

static void save_path(char *buf, size_t n)
{
    const char *test = SDL_getenv("ALF_SAVE");   /* tests keep their own save file */
    if (test) { SDL_strlcpy(buf, test, n); return; }
    app_save_path(buf, n, "stan.sav");
}

static void status(const char *m) { SDL_strlcpy(status_msg, m, sizeof status_msg); status_frames = 150; }

/* game thread, right after a vbl: the original's own state block, like its save disk */
static void game_vbl_hook(void)
{
    int r = g_req;
    if (!r) return;
    g_req = 0;
    if (!amiga_main_loaded()) {               /* the intro has no game state yet */
        status("Najpierw intro");
        return;
    }
    char path[1024];
    save_path(path, sizeof path);
    if (r == 1) {
        status(SDL_SaveFile(path, mem + STATE_ADDR, STATE_SIZE) ? "Zapisano" : "Błąd zapisu");
    } else {
        size_t len;
        void *data = SDL_LoadFile(path, &len);
        if (!data || len != STATE_SIZE) { status("Brak zapisu"); SDL_free(data); return; }
        memcpy(mem + STATE_ADDR, data, STATE_SIZE);
        SDL_free(data);
        vm_request_jump(0x173F0);     /* restore_after_load: music, panels, fade, resume_location */
        status("Wczytano");
    }
}

/* ------------------------------------------------------------------ API */

void panels_quick_save(void) { g_req = 1; }
void panels_quick_load(void) { g_req = 2; }

void panels_init(void)
{
    memcpy(font, mem + 0x95FAC, sizeof font);
    decode_ui();
    build_panels();
    build_title();
    amiga_set_vbl_hook(game_vbl_hook);
}

void panels_compose(uint32_t *canvas, const uint32_t *game, bool panels)
{
    static uint32_t ov[SCREEN_W * SCREEN_H];
    const uint32_t *src = game;
    if (overlay == OV_TITLE) src = title_rgb;
    else if (overlay == OV_CREDITS) { render_credits(ov); src = ov; }
    else if (overlay == OV_TOOLS) { render_tools(ov); src = ov; }
    for (int y = 0; y < SCREEN_H; y++)
        memcpy(canvas + y * CANVAS_W + PANEL_W, src + y * SCREEN_W, SCREEN_W * 4);
    if (overlay == OV_NONE && natives_title_shown()) {
        /* the intro's title picture: add the port line underneath, like the mockup */
        static uint32_t t[SCREEN_W * SCREEN_H];
        memcpy(t, game, sizeof t);
        centred(t, SCREEN_W, SCREEN_H, 360, 234, "Port by Pete Roman, for Seven Stars Multimedia", 0xFFB6B6B6, 1, 1);
        centred(t, SCREEN_W, SCREEN_H, 360, 244, "and Maciej 'Maniak' Frontkiewicz, with love", 0xFFB6B6B6, 1, 1);
        for (int y = 0; y < SCREEN_H; y++)
            memcpy(canvas + y * CANVAS_W + PANEL_W, t + y * SCREEN_W, SCREEN_W * 4);
    }
    if (panels) {
        draw_left_panel(canvas);
        for (int y = 0; y < SCREEN_H; y++)
            memcpy(canvas + y * CANVAS_W + PANEL_W + SCREEN_W, right_panel + y * PANEL_W, PANEL_W * 4);
    }
}

static int item_at(int cx, int cy)
{
    if (cx < ITEM_X || cx >= ITEM_X + 108) return -1;
    for (int i = 0; i < N_ITEMS; i++)
        if (cy >= ITEM_Y0 + i * ITEM_DY && cy < ITEM_Y0 + i * ITEM_DY + 12) return i;
    return -1;
}

void panels_hover(int cx, int cy, bool panels)
{
    hover_item = panels ? item_at(cx, cy) : -1;
}

static void open_overlay(int which)
{
    overlay = which;
    page = 0;
}

/* clicks inside the game area while an overlay is shown advance / close it */
bool panels_overlay_click(void)
{
    if (overlay == OV_NONE) return false;
    if (overlay == OV_TITLE) {
        overlay = OV_NONE;
        audio_stop_intro();
    } else if (overlay == OV_CREDITS) {
        if (++page >= N_CREDIT_PAGES) overlay = OV_NONE;
    } else if (overlay == OV_TOOLS) {
        if (++page >= N_TOOL_PAGES) overlay = OV_NONE;
    }
    return true;
}

void panels_click(int cx, int cy, bool panels, bool down)
{
    if (!down || !panels) return;
    int i = item_at(cx, cy);
    switch (i) {
    case 0:
        volume_level = cx >= ITEM_X + 62 ? SDL_clamp((cx - ITEM_X - 62) / 7 + 1, 0, 6) : (volume_level + 1) % 7;
        audio_set_volume(volume_level / 6.0f);
        break;
    case 1: g_scale_choice = g_scale_choice >= 4 ? 0 : g_scale_choice + 1; break;
    case 2: g_fullscreen = !g_fullscreen; break;
    case 3: if (overlay == OV_NONE) g_req = 1; break;
    case 4: if (overlay == OV_NONE) g_req = 2; break;
    case 5: open_overlay(OV_CREDITS); break;
    case 6: open_overlay(OV_TOOLS); break;
    case 7: quit_req = true; break;
    default: break;
    }
}

bool panels_quit_requested(void) { return quit_req; }
bool panels_overlay_active(void) { return overlay != OV_NONE; }
int  panels_scale_choice(void) { return g_scale_choice; }
bool panels_fullscreen_wanted(void) { return g_fullscreen; }
void panels_set_fullscreen(bool fs) { g_fullscreen = fs; }
