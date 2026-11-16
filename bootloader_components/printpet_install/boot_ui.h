/* SPDX-License-Identifier: MIT */
#pragma once

/* Call begin only for an incomplete installation marker. No RTOS, heap,
 * framebuffer, hardware SPI, USB or UART configuration is used. */
void boot_ui_begin(void);
void boot_ui_progress(unsigned percent);
/* Initializes the screen if needed. The numeric code belongs in serial logs,
 * not in the first-use screen. */
void boot_ui_fail(unsigned code);
