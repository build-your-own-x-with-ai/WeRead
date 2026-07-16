/**
 * Content Renderer Implementation
 *
 * Renders content blocks to displayable text with CJK-aware wrapping.
 * CJK characters count as 2 columns, Latin characters as 1.
 */

#include "content_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUTPUT_BUF_INITIAL (128 * 1024)

/* Get display width of a single UTF-8 codepoint */
static int get_char_display_width(const char *p, int *bytes_consumed)
{
    unsigned char c = (unsigned char)*p;

    if (c < 0x80) {
        *bytes_consumed = 1;
        if (c == '\n' || c == '\t') return 1;
        if (c < 0x20) return 0;
        return 1;
    }

    /* Decode UTF-8 */
    uint32_t codepoint = 0;
    if ((c & 0xE0) == 0xC0) {
        *bytes_consumed = 2;
        codepoint = (c & 0x1F) << 6;
        if (*(p+1)) codepoint |= ((unsigned char)*(p+1)) & 0x3F;
    } else if ((c & 0xF0) == 0xE0) {
        *bytes_consumed = 3;
        codepoint = (c & 0x0F) << 12;
        if (*(p+1)) codepoint |= (((unsigned char)*(p+1)) & 0x3F) << 6;
        if (*(p+2)) codepoint |= ((unsigned char)*(p+2)) & 0x3F;
    } else if ((c & 0xF8) == 0xF0) {
        *bytes_consumed = 4;
        codepoint = (c & 0x07) << 18;
        if (*(p+1)) codepoint |= (((unsigned char)*(p+1)) & 0x3F) << 12;
        if (*(p+2)) codepoint |= (((unsigned char)*(p+2)) & 0x3F) << 6;
        if (*(p+3)) codepoint |= ((unsigned char)*(p+3)) & 0x3F;
    } else {
        *bytes_consumed = 1;
        return 1;
    }

    /* CJK ranges: 2 columns wide */
    if ((codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||   /* CJK Unified */
        (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||   /* CJK Extension A */
        (codepoint >= 0x3000 && codepoint <= 0x303F) ||   /* CJK Symbols */
        (codepoint >= 0x3040 && codepoint <= 0x309F) ||   /* Hiragana */
        (codepoint >= 0x30A0 && codepoint <= 0x30FF) ||   /* Katakana */
        (codepoint >= 0xFF00 && codepoint <= 0xFF60) ||   /* Fullwidth */
        (codepoint >= 0xFFE0 && codepoint <= 0xFFE6) ||   /* Fullwidth Signs */
        (codepoint >= 0xAC00 && codepoint <= 0xD7AF) ||   /* Hangul */
        (codepoint >= 0x20000 && codepoint <= 0x2A6DF) || /* CJK Extension B */
        (codepoint >= 0x2E80 && codepoint <= 0x2EFF) ||   /* CJK Radicals Sup */
        (codepoint >= 0x2F00 && codepoint <= 0x2FDF) ||   /* Kangxi Radicals */
        (codepoint >= 0xFE30 && codepoint <= 0xFE4F)) {   /* CJK Compat Forms */
        return 2;
    }

    return 1;
}

typedef struct {
    char *buf;
    size_t size;
    size_t capacity;
} output_buf_t;

static void out_init(output_buf_t *ob)
{
    ob->capacity = OUTPUT_BUF_INITIAL;
    ob->buf = malloc(ob->capacity);
    ob->size = 0;
    ob->buf[0] = '\0';
}

static void out_ensure(output_buf_t *ob, size_t needed)
{
    while (ob->size + needed >= ob->capacity) {
        ob->capacity *= 2;
        ob->buf = realloc(ob->buf, ob->capacity);
    }
}

static void out_append(output_buf_t *ob, const char *str, size_t len)
{
    out_ensure(ob, len + 1);
    memcpy(ob->buf + ob->size, str, len);
    ob->size += len;
    ob->buf[ob->size] = '\0';
}

static void out_append_char(output_buf_t *ob, char c)
{
    out_ensure(ob, 2);
    ob->buf[ob->size++] = c;
    ob->buf[ob->size] = '\0';
}

static void out_append_newline(output_buf_t *ob)
{
    out_append_char(ob, '\n');
}

static void out_append_utf8_char(output_buf_t *ob, const char *p, int bytes)
{
    out_ensure(ob, bytes + 1);
    memcpy(ob->buf + ob->size, p, bytes);
    ob->size += bytes;
    ob->buf[ob->size] = '\0';
}

/* Wrap text to given width, appending to output buffer */
static void wrap_and_append(output_buf_t *ob, const char *text, int width, const char *prefix)
{
    if (!text || !*text) {
        if (prefix) out_append(ob, prefix, strlen(prefix));
        out_append_newline(ob);
        return;
    }

    int prefix_len = prefix ? (int)strlen(prefix) : 0;
    int current_width = prefix_len;

    if (prefix) {
        out_append(ob, prefix, prefix_len);
    }

    const char *p = text;
    while (*p) {
        if (*p == '\n') {
            out_append_newline(ob);
            current_width = 0;
            if (prefix && *(p + 1)) {
                out_append(ob, prefix, prefix_len);
                current_width = prefix_len;
            }
            p++;
            continue;
        }

        int bytes;
        int char_width = get_char_display_width(p, &bytes);

        if (current_width + char_width > width && current_width > prefix_len) {
            out_append_newline(ob);
            current_width = 0;
            if (prefix) {
                out_append(ob, prefix, prefix_len);
                current_width = prefix_len;
            }
        }

        out_append_utf8_char(ob, p, bytes);
        current_width += char_width;
        p += bytes;
    }
    out_append_newline(ob);
}

char *render_blocks_to_text(content_block_t *blocks, int count, int width)
{
    if (!blocks || count <= 0) return strdup("");

    output_buf_t ob;
    out_init(&ob);

    for (int i = 0; i < count; i++) {
        content_block_t *block = &blocks[i];

        switch (block->kind) {
            case BLOCK_H1:
                wrap_and_append(&ob, block->text, width, "# ");
                break;

            case BLOCK_H2:
                wrap_and_append(&ob, block->text, width, "## ");
                break;

            case BLOCK_H3:
                wrap_and_append(&ob, block->text, width, "### ");
                break;

            case BLOCK_H4:
                wrap_and_append(&ob, block->text, width, "#### ");
                break;

            case BLOCK_BLOCKQUOTE:
                wrap_and_append(&ob, block->text, width - 2, "  ");
                break;

            case BLOCK_HR: {
                /* Draw a horizontal rule line */
                int hr_width = width < 60 ? width : 60;
                for (int j = 0; j < hr_width; j++) {
                    out_append_char(&ob, '-');
                }
                out_append_newline(&ob);
                break;
            }

            case BLOCK_IMAGE:
                /* Images are skipped in parser, but handle gracefully */
                break;

            case BLOCK_PRE:
                wrap_and_append(&ob, block->text, width, "  ");
                break;

            case BLOCK_P:
                if (block->text && block->text[0]) {
                    wrap_and_append(&ob, block->text, width, NULL);
                }
                break;

            case BLOCK_EMPTY:
                /* No extra blank lines */
                break;
        }
    }

    return ob.buf;
}

char *html_to_display_text(const char *html, int width)
{
    if (!html) return strdup("");

    content_block_t *blocks = NULL;
    int count = 0;

    if (parse_html_to_blocks(html, &blocks, &count) != 0) {
        return strdup("[Error parsing content]");
    }

    char *result = render_blocks_to_text(blocks, count, width);
    content_blocks_free(blocks, count);

    return result;
}
