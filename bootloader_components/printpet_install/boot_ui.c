/* SPDX-License-Identifier: MIT
 * Minimal first-install screen for the verified AI Passport ST7789P3 panel.
 * BSP source: components/bsp/include/bsp_pins.h and src/bsp_display.c.
 * GPIO18/19 and USB console routing are deliberately untouched. Only the
 * selected display pads are made GPIO outputs; no UART APIs/input routes.
 */
#include "boot_ui.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "hal/gpio_ll.h"
#include "hal/wdt_hal.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_sig_map.h"

enum {
    LCD_CS = 1, LCD_CLK = 8, LCD_MOSI = 9, LCD_DC = 20, LCD_BL = 21,
    WIDTH = 240, HEIGHT = 320,
    BACKGROUND = 0x0841, TEXT = 0xef7d, ACCENT = 0x07d0,
    BORDER = 0x39e7, ERROR = 0xf800,
};

static bool ready, failed;
static uint8_t last_percent = 255;

static void display_feed(void)
{
    wdt_hal_context_t wdt = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&wdt);
    wdt_hal_feed(&wdt);
    wdt_hal_write_protect_enable(&wdt);
}

/* Five columns, low bit at the top. Digits 0..9, then uppercase A..Z.
 * Drawn directly as RGB565 pixels; no framebuffer or row allocation. */
static const uint8_t glyphs[36][5] = {
    {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e},
    {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22}, {0x7f,0x41,0x41,0x22,0x1c},
    {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01},
    {0x3e,0x41,0x49,0x49,0x7a}, {0x7f,0x08,0x08,0x08,0x7f},
    {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01},
    {0x7f,0x08,0x14,0x22,0x41}, {0x7f,0x40,0x40,0x40,0x40},
    {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f},
    {0x3e,0x41,0x41,0x41,0x3e}, {0x7f,0x09,0x09,0x09,0x06},
    {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7f,0x01,0x01},
    {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f},
    {0x3f,0x40,0x38,0x40,0x3f}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
};

/* cmd, parameter count, parameters. Identical vendor values to the BSP;
 * the final NOP/zero pair terminates the stream. */
static const uint8_t vendor_init[] = {
    0xb2,5,0x05,0x05,0x00,0x33,0x33,
    0xb7,1,0x35, 0xbb,1,0x21, 0xc0,1,0x2c, 0xc2,1,0x01,
    0xc3,1,0x0b, 0xc4,1,0x20, 0xc6,1,0x0f,
    0xd0,2,0xa7,0xa1, 0xd0,2,0xa4,0xa1, 0xd6,1,0xa1,
    0xe0,14,0xd0,0x04,0x08,0x0a,0x09,0x05,0x2d,0x43,
            0x49,0x09,0x16,0x15,0x26,0x2b,
    0xe1,14,0xd0,0x03,0x09,0x0a,0x0a,0x06,0x2e,0x44,
            0x40,0x3a,0x15,0x15,0x26,0x2a,
    0,0,
};

static inline void level(unsigned pin, unsigned value)
{
    gpio_ll_set_level(&GPIO, pin, value);
}

static void write_byte(uint8_t value)
{
    for (unsigned mask = 0x80; mask; mask >>= 1) {
        level(LCD_CLK, 0);
        level(LCD_MOSI, (value & mask) != 0);
        level(LCD_CLK, 1);
    }
    level(LCD_CLK, 0);
}

static void command(uint8_t value)
{
    level(LCD_CS, 0);
    level(LCD_DC, 0);
    write_byte(value);
    level(LCD_CS, 1);
}

static void data_begin(void)
{
    level(LCD_DC, 1);
    level(LCD_CS, 0);
}

static void data_end(void)
{
    level(LCD_CS, 1);
    level(LCD_CLK, 0);
    level(LCD_MOSI, 1);
}

static void parameter(uint8_t value)
{
    data_begin(); write_byte(value); data_end();
}

static void pixel(uint16_t color)
{
    write_byte((uint8_t)(color >> 8));
    write_byte((uint8_t)color);
}

static bool window(unsigned x, unsigned y, unsigned width, unsigned height)
{
    if (!width || !height || x >= WIDTH || y >= HEIGHT ||
        width > WIDTH - x || height > HEIGHT - y) return false;
    unsigned x_end = x + width - 1, y_end = y + height - 1;
    command(0x2a); data_begin();
    write_byte((uint8_t)(x >> 8)); write_byte((uint8_t)x);
    write_byte((uint8_t)(x_end >> 8)); write_byte((uint8_t)x_end); data_end();
    command(0x2b); data_begin();
    write_byte((uint8_t)(y >> 8)); write_byte((uint8_t)y);
    write_byte((uint8_t)(y_end >> 8)); write_byte((uint8_t)y_end); data_end();
    command(0x2c); data_begin();
    return true;
}

static void rectangle(unsigned x, unsigned y, unsigned width, unsigned height, uint16_t color)
{
    if (!window(x, y, width, height)) return;
    for (unsigned row = 0; row < height; ++row) {
        display_feed();
        for (unsigned col = 0; col < width; ++col) pixel(color);
    }
    data_end();
}

static uint8_t column(char c, unsigned col)
{
    if (col >= 5) return 0;
    if (c >= '0' && c <= '9') return glyphs[(unsigned)(c - '0')][col];
    if (c >= 'A' && c <= 'Z') return glyphs[10 + (unsigned)(c - 'A')][col];
    if (c == '%') {
        static const uint8_t percent[5] = {0x63,0x13,0x08,0x64,0x63};
        return percent[col];
    }
    return 0;
}

static void text(unsigned y, const char *value, unsigned scale, uint16_t color)
{
    unsigned count = 0;
    while (value[count]) ++count;
    unsigned cell = 6 * scale, width = count * cell, height = 8 * scale;
    if (!scale || width > WIDTH || !window((WIDTH - width) / 2, y, width, height)) return;
    for (unsigned py = 0; py < height; ++py) {
        display_feed();
        for (unsigned px = 0; px < width; ++px) {
            unsigned bit = py / scale;
            uint8_t glyph = column(value[px / cell], (px % cell) / scale);
            pixel((glyph & (1u << bit)) ? color : BACKGROUND);
        }
    }
    data_end();
}

static void output(unsigned pin, unsigned initial)
{
    /* Only the five display pads are selected. In particular no function that
     * changes USB pads, global pad holds or a UART input route is called. */
    esp_rom_gpio_pad_unhold(pin);
    level(pin, initial);
    esp_rom_gpio_pad_select_gpio(pin);
    esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
    gpio_ll_od_disable(&GPIO, pin);
    gpio_ll_output_enable(&GPIO, pin);
}

void boot_ui_begin(void)
{
    if (ready) return;
    output(LCD_BL, 0);
    output(LCD_CS, 1); output(LCD_CLK, 0);
    output(LCD_MOSI, 1); output(LCD_DC, 0);
    command(0x01); esp_rom_delay_us(20000);  /* BSP software reset delay */
    command(0x11); esp_rom_delay_us(100000); /* BSP sleep-out delay */
    command(0x36); parameter(0x00);         /* RGB, no mirror/swap */
    command(0x3a); parameter(0x55);         /* RGB565 */
    command(0xb0); data_begin(); write_byte(0x00); write_byte(0xf0); data_end();
    const uint8_t *p = vendor_init;
    while (p[0]) {
        uint8_t cmd = *p++, length = *p++;
        command(cmd); data_begin();
        for (unsigned n = 0; n < length; ++n) write_byte(*p++);
        data_end();
    }
    esp_rom_delay_us(10000);
    command(0x21);                         /* BSP panel inversion */
    command(0x29);                         /* display on */
    rectangle(0, 0, WIDTH, HEIGHT, BACKGROUND);
    text(76, "PREPARING", 3, TEXT);
    text(126, "KEEP POWER ON", 2, TEXT);
    rectangle(24, 184, 192, 16, BORDER);
    rectangle(26, 186, 188, 12, BACKGROUND);
    ready = true;
    last_percent = 255;
    boot_ui_progress(0);
    level(LCD_BL, 1);                      /* GPIO output only, not UART/LEDC */
}

void boot_ui_progress(unsigned percent)
{
    if (!ready || failed) return;
    if (percent > 100) percent = 100;
    if (percent == last_percent) return;
    last_percent = (uint8_t)percent;
    unsigned filled = (188 * percent) / 100;
    if (filled) rectangle(26, 186, filled, 12, ACCENT);
    if (filled < 188) rectangle(26 + filled, 186, 188 - filled, 12, BACKGROUND);
    char value[5] = {
        percent >= 100 ? '1' : ' ',
        percent >= 10 ? (char)('0' + (percent / 10) % 10) : ' ',
        (char)('0' + percent % 10), '%', 0
    };
    text(220, value, 3, TEXT);
}

void boot_ui_fail(unsigned code)
{
    (void)code;
    if (!ready) boot_ui_begin();
    failed = true;
    rectangle(0, 64, WIDTH, 192, BACKGROUND);
    text(80, "SETUP STOPPED", 2, ERROR);
    text(126, "REINSTALL", 2, ERROR);
}
