/* Aftershock: terminal text drawn over the picture (HUD, messages, menus).
 *
 * The engine thread describes what text to show; the render thread lays it
 * over the picture cells just before they are diffed and sent.  Text is
 * real terminal characters, readable at any size, where the engine's own
 * status bar and fonts would be a few blurred pixels at 100x30. */
#ifndef AV_OVERLAY_H
#define AV_OVERLAY_H

#include <stddef.h>
#include <stdint.h>

/* Render thread: lay the current overlay over a cell grid.  top/bot/ch point
 * at the first cell's fields; stride is the size of one cell in bytes.
 * For a text cell, top becomes the ink colour, bot the paper colour and ch
 * the character. */
void av_overlay_apply_cells(int cols, int rows, uint32_t *top, uint32_t *bot,
                            uint32_t *ch, size_t stride);

#endif
