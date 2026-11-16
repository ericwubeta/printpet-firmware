// Da Fei Yu portrait with the original terminal-style startup presentation.
#include "app_boot.h"
#include "app_identity.h"
#include "../assets/images/branding/fish-boot-v1/variants/112/fish_boot_img.h"
#include <string.h>
#include "esp_log.h"
#include "font_zh.h"
#include "lvgl.h"

#define BG 0x04070A
#define PANEL 0x06120F
#define BORDER 0x0E4A43
#define NEON 0x19E3C0
#define ACCENT 0xFF4D8D
#define DIM 0x6A9A90

static const char *const lines[] = {
    "> boot v" FW_VERSION,
    "> wake spirit ...",
    "> prepare display",
    "> load interface",
    "> run PrintPet()",
};

typedef struct {
    lv_obj_t *screen, *log, *command, *cursor, *bar, *ready;
    lv_timer_t *timer;
    void (*done)(void);
    uint32_t started, bar_started, pause_until;
    unsigned line, character;
    bool bar_active, cursor_on;
    char body[160], execution[48];
} boot_context_t;
static boot_context_t ctx;

static void cancel_tick(void)
{
    if (ctx.timer) { lv_timer_delete(ctx.timer); ctx.timer = NULL; }
}

static void deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != ctx.screen) return;
    cancel_tick();
    memset(&ctx, 0, sizeof(ctx));
}

static void finish(void)
{
    lv_obj_t *screen = ctx.screen;
    void (*done)(void) = ctx.done;
    cancel_tick();
    memset(&ctx, 0, sizeof(ctx));
    if (!done) { ctx.screen = screen; return; }
    ESP_LOGI("app_boot", "fish terminal splash complete");
    done();
    if (screen && lv_obj_is_valid(screen) && screen != lv_screen_active())
        lv_obj_delete(screen);
}

static bool append(char *buffer, size_t capacity, char value)
{
    size_t used = strlen(buffer);
    if (used + 1 >= capacity) return false;
    buffer[used] = value; buffer[used + 1] = 0;
    return true;
}

static void type_character(uint32_t elapsed)
{
    if (ctx.line >= 5 || elapsed < ctx.pause_until) return;
    const char *line = lines[ctx.line];
    if (ctx.line < 4) {
        if (!ctx.character && ctx.line && !append(ctx.body, sizeof(ctx.body), '\n')) return;
        if (!append(ctx.body, sizeof(ctx.body), line[ctx.character])) return;
        lv_label_set_text(ctx.log, ctx.body);
    } else {
        if (!append(ctx.execution, sizeof(ctx.execution), line[ctx.character])) return;
        lv_label_set_text(ctx.command, ctx.execution);
    }
    if (line[++ctx.character] == 0) {
        ++ctx.line; ctx.character = 0; ctx.pause_until = elapsed + 90;
        if (ctx.line == 5) {
            ctx.bar_active = true; ctx.bar_started = elapsed;
            lv_obj_set_style_opa(ctx.cursor, LV_OPA_TRANSP, 0);
        }
    }
}

static void tick(lv_timer_t *timer)
{
    if (timer != ctx.timer || !ctx.screen) return;
    uint32_t elapsed = lv_tick_elaps(ctx.started);
    if (elapsed >= 4500) { finish(); return; }
    if (!ctx.bar_active) {
        type_character(elapsed); type_character(elapsed);
        if (!ctx.bar_active) {
            bool on = (elapsed / 250) % 2 == 0;
            if (on != ctx.cursor_on) {
                ctx.cursor_on = on;
                lv_obj_set_style_opa(ctx.cursor, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            }
        }
    } else {
        uint32_t age = elapsed - ctx.bar_started;
        lv_obj_set_width(ctx.bar, age >= 700 ? 144 : (int32_t)(144 * age / 700));
        if (age >= 700) lv_obj_set_style_opa(ctx.ready, LV_OPA_COVER, 0);
        if (age >= 1300) { finish(); return; }
    }
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int width, int height, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    if (!o) return NULL;
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, width, height);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(o, 0, 0); lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int width, const char *text, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    if (!o) return NULL;
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, width);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_text(o, text);
    return o;
}

void app_boot_show(void (*done)(void))
{
    lv_obj_t *previous = ctx.screen;
    cancel_tick(); memset(&ctx, 0, sizeof(ctx)); ctx.done = done;
    ctx.screen = lv_obj_create(NULL);
    if (!ctx.screen) { finish(); return; }
    lv_obj_remove_flag(ctx.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ctx.screen, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(ctx.screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ctx.screen, 0, 0);
    lv_obj_set_style_pad_all(ctx.screen, 0, 0);
    lv_obj_add_event_cb(ctx.screen, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *portrait = lv_image_create(ctx.screen);
    lv_obj_t *brand = label(ctx.screen, 126, 40, 104,
                           APP_PRODUCT_FAMILY "\n" APP_PRODUCT_LINE, 0xE6EDF3);
    lv_obj_t *panel = box(ctx.screen, 16, 126, 208, 132, PANEL);
    if (!portrait || !brand || !panel) { finish(); return; }
    lv_image_set_src(portrait, &fish_boot_img); lv_obj_set_pos(portrait, 4, 8);
    lv_obj_set_style_text_font(brand, &font_zh20, 0);
    lv_obj_set_style_text_line_space(brand, 4, 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(BORDER), 0);
    lv_obj_set_style_shadow_color(panel, lv_color_hex(NEON), 0);
    lv_obj_set_style_shadow_width(panel, 6, 0);
    lv_obj_set_style_shadow_opa(panel, 30, 0);
    if (!label(panel, 8, 6, 192, "PRINTPET // BOOT", DIM)) { finish(); return; }
    ctx.log = label(panel, 8, 27, 192, "", NEON);
    ctx.command = label(panel, 8, 98, 192, "", ACCENT);
    ctx.cursor = box(panel, 8, 115, 7, 11, NEON);
    lv_obj_t *track = box(ctx.screen, 46, 274, 148, 12, BORDER);
    if (!ctx.log || !ctx.command || !ctx.cursor || !track) { finish(); return; }
    lv_obj_set_style_radius(ctx.cursor, 0, 0);
    lv_obj_set_style_radius(track, 3, 0);
    ctx.bar = box(track, 2, 1, 0, 10, NEON);
    ctx.ready = label(ctx.screen, 0, 294, 240, "READY", NEON);
    if (!ctx.bar || !ctx.ready) { finish(); return; }
    lv_obj_set_style_radius(ctx.bar, 0, 0);
    lv_obj_set_style_text_align(ctx.ready, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(ctx.ready, 3, 0);
    lv_obj_set_style_opa(ctx.ready, LV_OPA_TRANSP, 0);
    lv_screen_load(ctx.screen);
    if (previous && lv_obj_is_valid(previous) && previous != ctx.screen) lv_obj_delete(previous);
    ctx.started = lv_tick_get(); ctx.cursor_on = true;
    ctx.timer = lv_timer_create(tick, 16, NULL);
    if (!ctx.timer) { finish(); return; }
    ESP_LOGI("app_boot", "fish terminal splash start v%s", FW_VERSION);
}
