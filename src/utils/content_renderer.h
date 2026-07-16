/**
 * Content Renderer - Renders content blocks to displayable text
 * with CJK-aware character width and text wrapping
 */

#ifndef CONTENT_RENDERER_H
#define CONTENT_RENDERER_H

#include "html_parser.h"

/**
 * Render content blocks into a single displayable text string.
 * Handles CJK character width (2 for CJK, 1 for Latin), heading prefixes,
 * blockquote indentation, HR lines, and image labels.
 *
 * @param blocks Array of content blocks
 * @param count Number of blocks
 * @param width Display width in characters for text wrapping
 * @return Newly allocated string (caller must free), or NULL on error
 */
char *render_blocks_to_text(content_block_t *blocks, int count, int width);

/**
 * Parse HTML directly to displayable text (convenience function).
 * Combines parse_html_to_blocks + render_blocks_to_text.
 *
 * @param html Input HTML string
 * @param width Display width for wrapping
 * @return Newly allocated displayable text (caller must free)
 */
char *html_to_display_text(const char *html, int width);

#endif /* CONTENT_RENDERER_H */
