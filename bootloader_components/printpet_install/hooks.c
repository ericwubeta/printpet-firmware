/* SPDX-License-Identifier: MIT
 * First-boot decompressor. All mutations are restricted to the pinned output
 * and the completion record. Never erase/write the factory identity gap.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "bootloader_flash_priv.h"
#include "bootloader_sha.h"
#include "esp_rom_sys.h"
#include "hal/wdt_hal.h"
#include "miniz.h"
#include "boot_ui.h"

typedef struct {
    uint32_t source, destination, packed_bytes, raw_bytes;
    uint8_t packed_sha[32], raw_sha[32];
} pp_image_t;
#include "payload.h"

static tinfl_decompressor decoder;
static uint8_t ring[PP_WINDOW] __attribute__((aligned(4)));
static uint8_t input[128] __attribute__((aligned(4)));
static const uint8_t complete[16] __attribute__((aligned(4))) = "PC161-installed";
static unsigned install_stage;
static unsigned install_image;

void bootloader_hooks_include(void) {}

static void feed(void)
{
    wdt_hal_context_t wdt = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&wdt);
    wdt_hal_feed(&wdt);
    wdt_hal_write_protect_enable(&wdt);
}

static void stop(void)
{
    boot_ui_fail(install_image * 100 + install_stage);
    esp_rom_printf("PrintPet installation stopped safely (image=%u stage=%u). Reinstall the same community file.\n", install_image, install_stage);
    for (;;) { feed(); esp_rom_delay_us(10000); }
}

static uint32_t aligned(uint32_t size, uint32_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

/* Overflow-safe whitelist, checked immediately before EVERY erase/write. */
static bool writable(uint32_t addr, uint32_t count)
{
    if (!count || addr > 0x800000 || count > 0x800000 - addr) return false;
    uint32_t end = addr + count;
    if (addr < 0x370000 && end > 0x310000) return false;
    if (addr >= PP_DONE_OFFSET && end <= PP_DONE_OFFSET + 16) return true;
    for (unsigned i = 0; i < PP_IMAGE_COUNT; ++i) {
        const pp_image_t *p = &images[i];
        if (addr >= p->destination && end <= p->destination + aligned(p->raw_bytes, 4096))
            return true;
    }
    return false;
}

static bool hash_is(uint32_t addr, uint32_t bytes, const uint8_t expected[32], bool *matches)
{
    uint8_t result[32];
    bootloader_sha256_handle_t sha = bootloader_sha256_start();
    for (uint32_t pos = 0; pos < bytes;) {
        uint32_t n = bytes - pos;
        if (n > sizeof(input)) n = sizeof(input);
        feed();
        if (bootloader_flash_read(addr + pos, input, aligned(n, 4), false) != ESP_OK) {
            bootloader_sha256_finish(sha, NULL);
            return false;
        }
        bootloader_sha256_data(sha, input, n);
        pos += n;
    }
    bootloader_sha256_finish(sha, result);
    *matches = memcmp(result, expected, sizeof(result)) == 0;
    return true;
}

static bool erase_output(const pp_image_t *p)
{
    unsigned base = 0;
    unsigned half_span = 50;
    for (uint32_t pos = 0; pos < aligned(p->raw_bytes, 4096); pos += 4096) {
        uint32_t addr = p->destination + pos;
        feed();
        if (!writable(addr, 4096) || bootloader_flash_erase_range(addr, 4096) != ESP_OK)
            return false;
        boot_ui_progress(base + (uint64_t)half_span * (pos + 4096) / aligned(p->raw_bytes, 4096));
    }
    return true;
}

static bool install(const pp_image_t *p)
{
    bool matches;
    unsigned base = 0;
    unsigned half_span = 50;
    install_stage = 2;
    /* Reject damaged sources BEFORE any erasure. SHA values are compiled in. */
    if (!hash_is(p->source, p->packed_bytes, p->packed_sha, &matches) || !matches)
        return false;
    install_stage = 3;
    if (!erase_output(p)) return false;
    install_stage = 4;
    tinfl_init(&decoder);
    uint32_t consumed = 0, produced = 0;
    size_t in_pos = 0, in_bytes = 0;
    size_t ring_pos = 0;
    for (;;) {
        if (in_pos == in_bytes && consumed < p->packed_bytes) {
            in_bytes = p->packed_bytes - consumed;
            if (in_bytes > sizeof(input)) in_bytes = sizeof(input);
            if (bootloader_flash_read(p->source + consumed, input, aligned(in_bytes, 4), false) != ESP_OK)
                return false;
            in_pos = 0;
        }
        size_t n_in = in_bytes - in_pos, n_out = PP_WINDOW - ring_pos;
        uint32_t flags = TINFL_FLAG_PARSE_ZLIB_HEADER;
        if (consumed + n_in < p->packed_bytes) flags |= TINFL_FLAG_HAS_MORE_INPUT;
        feed();
        tinfl_status status = tinfl_decompress(&decoder, input + in_pos, &n_in,
                                               ring, ring + ring_pos, &n_out, flags);
        in_pos += n_in;
        consumed += n_in;
        if (ring_pos + n_out > p->raw_bytes - produced || status < 0) return false;
        /* Flush only full dictionary windows or the final tail, preserving
         * dictionary history and four-byte-aligned writes. Both raw sizes %4=0. */
        ring_pos += n_out;
        if (ring_pos == PP_WINDOW || status == TINFL_STATUS_DONE) {
            uint32_t n = ring_pos;
            uint32_t dest = p->destination + produced;
            if (n && ((n & 3) || !writable(dest, n) ||
                bootloader_flash_write(dest, ring, n, false) != ESP_OK)) return false;
            produced += n;
            ring_pos = 0;
            boot_ui_progress(base + half_span + (uint64_t)half_span * produced / p->raw_bytes);
        }
        if (status == TINFL_STATUS_DONE) {
            install_stage = 5;
            return consumed == p->packed_bytes && produced == p->raw_bytes &&
                   hash_is(p->destination, p->raw_bytes, p->raw_sha, &matches) && matches;
        }
        if (!n_in && !n_out) return false;
        if (consumed == p->packed_bytes && status == TINFL_STATUS_NEEDS_MORE_INPUT)
            return false;
    }
}

void bootloader_after_init(void)
{
    uint8_t marker[16] __attribute__((aligned(4)));
    if (bootloader_flash_read(PP_DONE_OFFSET, marker, sizeof(marker), false) != ESP_OK) stop();
    /* Completed installs hand off normally, including later app-only upgrades. */
    if (!memcmp(marker, complete, sizeof(marker))) return;
    boot_ui_begin();
    /* Incomplete programming of this exact marker can be safely retried. */
    for (unsigned i = 0; i < sizeof(marker); ++i)
        if ((marker[i] & complete[i]) != complete[i]) stop();
    /* Core has one application and no external resource payload. Its source
     * and destination never overlap, so an interrupted install can retry. */
    for (unsigned i = 0; i < PP_IMAGE_COUNT; ++i) {
        bool matches;
        const pp_image_t *p = &images[i];
        install_image = i;
        install_stage = 1;
        if (!hash_is(p->destination, p->raw_bytes, p->raw_sha, &matches)) stop();
        if (!matches && !install(p)) stop();
        boot_ui_progress(100);
    }
    install_stage = 6;
    if (!writable(PP_DONE_OFFSET, sizeof(complete)) ||
        bootloader_flash_write(PP_DONE_OFFSET, (void *)complete, sizeof(complete), false) != ESP_OK ||
        bootloader_flash_read(PP_DONE_OFFSET, marker, sizeof(marker), false) != ESP_OK ||
        memcmp(marker, complete, sizeof(marker))) stop();
    esp_rom_printf("PrintPet Core installation verified.\n");
}
