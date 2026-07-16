/**
 * HTML Parser - Converts HTML to content blocks
 */

#ifndef HTML_PARSER_H
#define HTML_PARSER_H

typedef enum {
    BLOCK_P,
    BLOCK_H1,
    BLOCK_H2,
    BLOCK_H3,
    BLOCK_H4,
    BLOCK_BLOCKQUOTE,
    BLOCK_IMAGE,
    BLOCK_HR,
    BLOCK_PRE,
    BLOCK_EMPTY
} block_kind_t;

typedef struct {
    char *text;
    block_kind_t kind;
} content_block_t;

/**
 * Parse HTML string into an array of content blocks.
 * @param html Input HTML string
 * @param blocks Output array (caller must free with content_blocks_free)
 * @param count Output block count
 * @return 0 on success, -1 on error
 */
int parse_html_to_blocks(const char *html, content_block_t **blocks, int *count);

/**
 * Free an array of content blocks
 */
void content_blocks_free(content_block_t *blocks, int count);

#endif /* HTML_PARSER_H */
