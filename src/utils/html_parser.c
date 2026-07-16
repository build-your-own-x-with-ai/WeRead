/**
 * HTML Parser Implementation
 *
 * Simple state-machine HTML parser that converts HTML to content blocks.
 * Handles block-level elements (p, h1-h4, blockquote, pre, hr, img)
 * and ignores inline formatting (b, i, em, strong, a, span, etc).
 */

#include "html_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_BLOCKS 4096
#define TEXT_BUF_SIZE 65536

typedef struct {
    content_block_t blocks[MAX_BLOCKS];
    int count;
    char text_buf[TEXT_BUF_SIZE];
    int text_pos;
    block_kind_t current_kind;
    int in_body;
    int skip_data;
    int in_pre;
} parser_state_t;

static void flush_block(parser_state_t *ps)
{
    /* Trim leading/trailing whitespace */
    while (ps->text_pos > 0 && isspace((unsigned char)ps->text_buf[ps->text_pos - 1]))
        ps->text_pos--;
    ps->text_buf[ps->text_pos] = '\0';

    /* Skip empty blocks unless it's HR or IMAGE */
    if (ps->text_pos == 0 && ps->current_kind != BLOCK_HR && ps->current_kind != BLOCK_IMAGE) {
        ps->text_buf[0] = '\0';
        ps->text_pos = 0;
        return;
    }

    if (ps->count >= MAX_BLOCKS) return;

    content_block_t *block = &ps->blocks[ps->count];
    block->text = strdup(ps->text_buf);
    block->kind = ps->text_pos > 0 ? ps->current_kind : BLOCK_EMPTY;
    ps->count++;

    ps->text_buf[0] = '\0';
    ps->text_pos = 0;
}

static void append_text(parser_state_t *ps, const char *text, int len)
{
    if (ps->skip_data || !ps->in_body) return;

    for (int i = 0; i < len && ps->text_pos < TEXT_BUF_SIZE - 2; i++) {
        char c = text[i];
        if (!ps->in_pre && isspace((unsigned char)c)) {
            /* Collapse whitespace */
            if (ps->text_pos > 0 && ps->text_buf[ps->text_pos - 1] != ' ') {
                ps->text_buf[ps->text_pos++] = ' ';
            }
        } else {
            ps->text_buf[ps->text_pos++] = c;
        }
    }
}

static int strcasecmp_n(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        int d = tolower((unsigned char)a[i]) - tolower((unsigned char)b[i]);
        if (d != 0) return d;
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static int is_block_tag(const char *tag, int len)
{
    if (len >= 1 && len <= 10) {
        if (strcasecmp_n(tag, "p", len) == 0 && len == 1) return 1;
        if (strcasecmp_n(tag, "div", len) == 0 && len == 3) return 1;
        if (strcasecmp_n(tag, "h1", len) == 0 && len == 2) return 1;
        if (strcasecmp_n(tag, "h2", len) == 0 && len == 2) return 1;
        if (strcasecmp_n(tag, "h3", len) == 0 && len == 2) return 1;
        if (strcasecmp_n(tag, "h4", len) == 0 && len == 2) return 1;
        if (strcasecmp_n(tag, "blockquote", len) == 0 && len == 10) return 1;
        if (strcasecmp_n(tag, "pre", len) == 0 && len == 3) return 1;
    }
    return 0;
}

static block_kind_t tag_to_kind(const char *tag, int len)
{
    if (len == 1 && strcasecmp_n(tag, "p", 1) == 0) return BLOCK_P;
    if (len == 2 && strcasecmp_n(tag, "h1", 2) == 0) return BLOCK_H1;
    if (len == 2 && strcasecmp_n(tag, "h2", 2) == 0) return BLOCK_H2;
    if (len == 2 && strcasecmp_n(tag, "h3", 2) == 0) return BLOCK_H3;
    if (len == 2 && strcasecmp_n(tag, "h4", 2) == 0) return BLOCK_H4;
    if (len == 10 && strcasecmp_n(tag, "blockquote", 10) == 0) return BLOCK_BLOCKQUOTE;
    if (len == 3 && strcasecmp_n(tag, "pre", 3) == 0) return BLOCK_PRE;
    if (len == 3 && strcasecmp_n(tag, "div", 3) == 0) return BLOCK_P;
    return BLOCK_P;
}

int parse_html_to_blocks(const char *html, content_block_t **blocks, int *count)
{
    if (!html || !blocks || !count) return -1;

    parser_state_t *ps = calloc(1, sizeof(parser_state_t));
    if (!ps) return -1;

    ps->in_body = 1;  /* Assume body if no <html>/<body> tags */
    ps->current_kind = BLOCK_P;

    /* ── Preprocess: strip leading XML declarations, DOCTYPE, comments ── */
    const char *p = html;
    const char *end = html + strlen(html);

    while (p < end) {
        /* Skip leading whitespace */
        while (p < end && isspace((unsigned char)*p)) p++;
        if (p >= end) break;

        if (p[0] == '<' && p + 1 < end) {
            if (p[1] == '?') {
                /* Processing instruction: <?...?> */
                const char *pi_end = strstr(p, "?>");
                if (pi_end && pi_end - p < 500) {
                    p = pi_end + 2;
                    continue;
                }
            } else if (p[1] == '!') {
                /* Comment: <!--...--> */
                if (p + 3 < end && p[2] == '-' && p[3] == '-') {
                    const char *c_end = strstr(p, "-->");
                    if (c_end && c_end - p < 1000) {
                        p = c_end + 3;
                        continue;
                    }
                }
                /* DOCTYPE or other declaration: <!...> */
                const char *d_end = strchr(p + 2, '>');
                if (d_end && d_end - p < 500) {
                    p = d_end + 1;
                    continue;
                }
            }
        }

        /*
         * Handle garbled XML declaration: decryption may corrupt the
         * leading "<?" to other characters (e.g. "@?", ">?"). Detect by
         * checking for XML-related keywords in the first 200 bytes.
         */
        if (p + 6 < end) {
            int scan_len = (int)(end - p);
            if (scan_len > 200) scan_len = 200;
            char scan[201];
            memcpy(scan, p, scan_len);
            scan[scan_len] = '\0';

            if (strstr(scan, "standalone=")) {
                const char *term = strstr(p, "?>");
                if (term && term - p < 500) {
                    p = term + 2;
                    /* Skip trailing whitespace/newline */
                    while (p < end && (*p == '\n' || *p == '\r')) p++;
                    continue;
                }
                /* No ?> terminator — find end of the declaration line */
                while (p < end && *p != '\n' && *p != '>') p++;
                if (p < end && *p == '>') p++;
                else if (p < end && *p == '\n') p++;
                continue;
            }
        }

        break; /* Not a declaration — start real parsing */
    }

    while (p < end) {
        if (*p == '<') {
            const char *tag_start = p + 1;
            const char *tag_end = strchr(tag_start, '>');
            if (!tag_end) {
                /* No closing >, treat rest as text */
                append_text(ps, p, (int)(end - p));
                break;
            }

            int is_closing = (*tag_start == '/');
            if (is_closing) tag_start++;

            /* Handle processing instructions: <?xml ... ?>, <?...?> */
            if (*tag_start == '?' || (!is_closing && *(p + 1) == '?')) {
                const char *pi_end = strstr(p, "?>");
                if (pi_end) {
                    p = pi_end + 2;
                } else {
                    p = tag_end + 1;
                }
                continue;
            }

            /* Handle declarations: <!DOCTYPE ...>, <!-- comments --> */
            if (*tag_start == '!') {
                const char *comment_end = strstr(p, "-->");
                if (comment_end) {
                    p = comment_end + 3;
                } else {
                    p = tag_end + 1;
                }
                continue;
            }

            /* Extract tag name */
            const char *name_end = tag_start;
            while (name_end < tag_end && *name_end != ' ' && *name_end != '/' && *name_end != '>')
                name_end++;
            int name_len = (int)(name_end - tag_start);

            /* Handle special tags */
            if (name_len == 4 && strcasecmp_n(tag_start, "body", 4) == 0) {
                if (!is_closing) ps->in_body = 1;
                else ps->in_body = 0;
            } else if (name_len == 4 && strcasecmp_n(tag_start, "html", 4) == 0) {
                ps->in_body = 0;
            } else if (name_len == 4 && strcasecmp_n(tag_start, "head", 4) == 0) {
                ps->in_body = 0;
                ps->skip_data = !is_closing;
            } else if (name_len == 5 && strcasecmp_n(tag_start, "style", 5) == 0) {
                ps->skip_data = !is_closing;
            } else if (name_len == 6 && strcasecmp_n(tag_start, "script", 6) == 0) {
                ps->skip_data = !is_closing;
            } else if (name_len == 2 && strcasecmp_n(tag_start, "br", 2) == 0 && !is_closing) {
                if (ps->text_pos < TEXT_BUF_SIZE - 1)
                    ps->text_buf[ps->text_pos++] = '\n';
            } else if (name_len == 2 && strcasecmp_n(tag_start, "hr", 2) == 0 && !is_closing) {
                flush_block(ps);
                if (ps->count < MAX_BLOCKS) {
                    ps->blocks[ps->count].text = strdup("");
                    ps->blocks[ps->count].kind = BLOCK_HR;
                    ps->count++;
                }
            } else if (name_len == 3 && strcasecmp_n(tag_start, "img", 3) == 0 && !is_closing) {
                /* Skip images entirely — LVGL cannot render them */
            } else if (!is_closing && is_block_tag(tag_start, name_len)) {
                /* Block opening tag: flush current text and start new block */
                flush_block(ps);
                ps->current_kind = tag_to_kind(tag_start, name_len);
                if (name_len == 3 && strcasecmp_n(tag_start, "pre", 3) == 0)
                    ps->in_pre = 1;
            } else if (is_closing && is_block_tag(tag_start, name_len)) {
                /* Block closing tag: flush and reset */
                flush_block(ps);
                ps->current_kind = BLOCK_P;
                if (name_len == 3 && strcasecmp_n(tag_start, "pre", 3) == 0)
                    ps->in_pre = 0;
            }
            /* Inline tags (b, i, em, strong, a, span, etc) are ignored */

            p = tag_end + 1;
        } else if (*p == '&') {
            /* Handle HTML entities */
            if (strncmp(p, "&amp;", 5) == 0) { append_text(ps, "&", 1); p += 5; }
            else if (strncmp(p, "&lt;", 4) == 0) { append_text(ps, "<", 1); p += 4; }
            else if (strncmp(p, "&gt;", 4) == 0) { append_text(ps, ">", 1); p += 4; }
            else if (strncmp(p, "&quot;", 6) == 0) { append_text(ps, "\"", 1); p += 6; }
            else if (strncmp(p, "&nbsp;", 6) == 0) { append_text(ps, " ", 1); p += 6; }
            else if (strncmp(p, "&#39;", 5) == 0) { append_text(ps, "'", 1); p += 5; }
            else { append_text(ps, "&", 1); p++; }
        } else {
            /* Regular text */
            const char *text_start = p;
            while (p < end && *p != '<' && *p != '&') p++;
            append_text(ps, text_start, (int)(p - text_start));
        }
    }

    /* Flush remaining text */
    flush_block(ps);

    /* Copy results */
    *count = ps->count;
    if (ps->count > 0) {
        *blocks = malloc(ps->count * sizeof(content_block_t));
        memcpy(*blocks, ps->blocks, ps->count * sizeof(content_block_t));
    } else {
        *blocks = NULL;
    }

    free(ps);
    return 0;
}

void content_blocks_free(content_block_t *blocks, int count)
{
    if (!blocks) return;
    for (int i = 0; i < count; i++) {
        free(blocks[i].text);
    }
    free(blocks);
}
