/**
 * Reader Screen Implementation
 *
 * Displays chapter content with navigation, boss mode, and fullscreen toggle.
 */

#include "screen_reader.h"
#include "screen_manager.h"
#include "../api/api_client.h"
#include "../utils/html_parser.h"
#include "../utils/content_renderer.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

extern lv_font_t *chinese_font;

/* Reader state */
static lv_obj_t *screen = NULL;
static lv_obj_t *header_label = NULL;
static lv_obj_t *content_label = NULL;
static lv_obj_t *scroll_container = NULL;
static lv_obj_t *footer_label = NULL;
static lv_obj_t *boss_overlay = NULL;
static lv_obj_t *header_container = NULL;
static lv_obj_t *footer_container = NULL;
static int image_count = 0;

static char reader_book_id[64] = {0};
static char reader_book_format[16] = "epub";
static int reader_chapter_uid = 0;

/* Chapter list for navigation */
#define MAX_READER_CHAPTERS 1000
typedef struct {
    int chapter_uid;
    char title[256];
} reader_chapter_t;

static reader_chapter_t reader_chapters[MAX_READER_CHAPTERS];
static int reader_chapter_count = 0;
static int reader_chapter_index = 0;

/* State flags */
static int boss_mode = 0;
static int fullscreen_mode = 0;

/* Toast notification for chapter boundary feedback */
static lv_obj_t *toast_label = NULL;
static lv_timer_t *toast_timer = NULL;

/* Boss mode fake content (Linux SWAP documentation) */
static const char *BOSS_FAKE_CONTENT =
    "SWAP 内存管理机制详解\n\n"
    "1. 概述\n\n"
    "Swap 是 Linux 系统中用于虚拟内存管理的重要机制。当物理内存(RAM)不足时，"
    "系统会将不常用的内存页面(page)交换到磁盘上的 swap 分区或 swap 文件中，"
    "从而释放物理内存供其他进程使用。\n\n"
    "2. Swap 分区配置\n\n"
    "  # 查看当前 swap 使用情况\n"
    "  $ free -h\n"
    "  $ swapon --show\n\n"
    "  # 创建 swap 文件\n"
    "  $ sudo fallocate -l 4G /swapfile\n"
    "  $ sudo chmod 600 /swapfile\n"
    "  $ sudo mkswap /swapfile\n"
    "  $ sudo swapon /swapfile\n\n"
    "3. 内核参数调优\n\n"
    "  vm.swappiness = 60     # 控制 swap 使用倾向 (0-100)\n"
    "  vm.vfs_cache_pressure = 100  # 控制 inode/dentry 缓存回收\n"
    "  vm.dirty_ratio = 20    # 脏页占内存比例上限\n"
    "  vm.dirty_background_ratio = 10  # 后台回写触发比例\n\n"
    "4. 性能监控\n\n"
    "  | 指标             | 命令                  |\n"
    "  |------------------|----------------------|\n"
    "  | Swap 使用率      | free -h              |\n"
    "  | 页面交换频率     | vmstat 1             |\n"
    "  | 各进程 swap 使用 | smem -s swap         |\n"
    "  | IO 等待          | iostat -x 1          |\n\n"
    "5. 最佳实践\n\n"
    "  - SSD 系统建议 swappiness=10\n"
    "  - 数据库服务器建议禁用或最小化 swap\n"
    "  - 容器环境中通常由宿主机统一管理\n"
    "  - 使用 zswap 或 zram 压缩内存页面\n\n"
    "6. 常见问题排查\n\n"
    "  Q: swap 使用率持续升高？\n"
    "  A: 检查是否有内存泄漏进程，使用 top/htop 按 MEM% 排序\n\n"
    "  Q: 系统频繁发生 swap in/out？\n"
    "  A: 说明内存严重不足，需要增加物理内存或优化应用内存使用\n\n"
    "参考文献: Linux Kernel Documentation, Red Hat Performance Guide\n";

static void toast_timeout_cb(lv_timer_t *t)
{
    (void)t;
    if (toast_label) {
        lv_obj_del(toast_label);
        toast_label = NULL;
    }
    if (toast_timer) {
        lv_timer_del(toast_timer);
        toast_timer = NULL;
    }
}

static void show_toast(const char *msg)
{
    /* Clean up existing toast */
    if (toast_timer) {
        lv_timer_del(toast_timer);
        toast_timer = NULL;
    }
    if (toast_label) {
        lv_obj_del(toast_label);
        toast_label = NULL;
    }

    /* Create centered toast with semi-transparent background */
    toast_label = lv_label_create(screen);
    lv_label_set_text(toast_label, msg);
    if (chinese_font) lv_obj_set_style_text_font(toast_label, chinese_font, 0);
    lv_obj_set_style_text_color(toast_label, lv_color_white(), 0);
    lv_obj_set_style_bg_color(toast_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(toast_label, LV_OPA_80, 0);
    lv_obj_set_style_radius(toast_label, 8, 0);
    lv_obj_set_style_pad_all(toast_label, 12, 0);
    lv_obj_align(toast_label, LV_ALIGN_CENTER, 0, 0);

    /* Auto-hide after 2 seconds */
    toast_timer = lv_timer_create(toast_timeout_cb, 2000, NULL);
    lv_timer_set_repeat_count(toast_timer, 1);
}

/* Forward declarations */
static void load_chapter_content(void);
static void load_chapter_list(void);
static void switch_chapter(int delta);
static void toggle_boss_mode(void);
static void toggle_fullscreen(void);

/* Keyboard event handler */
static void reader_key_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY) return;

    uint32_t key = lv_event_get_key(e);

    if (boss_mode) {
        /* In boss mode, only 'v' toggles it off */
        if (key == 'v' || key == 'V') {
            toggle_boss_mode();
        }
        return;
    }

    switch (key) {
        case LV_KEY_RIGHT:
        case '.':
            printf("[READER] Next chapter key pressed (index=%d/%d)\n",
                   reader_chapter_index, reader_chapter_count);
            switch_chapter(1);
            break;
        case LV_KEY_LEFT:
        case ',':
            printf("[READER] Prev chapter key pressed (index=%d/%d)\n",
                   reader_chapter_index, reader_chapter_count);
            switch_chapter(-1);
            break;
        case LV_KEY_DOWN:
            lv_obj_scroll_by(scroll_container, 0, 150, LV_ANIM_ON);
            break;
        case LV_KEY_UP:
            lv_obj_scroll_by(scroll_container, 0, -150, LV_ANIM_ON);
            break;
        case 'q':
        case 'Q':
            screen_manager_pop();
            break;
        case 'v':
        case 'V':
            toggle_boss_mode();
            break;
        case 'f':
        case 'F':
            toggle_fullscreen();
            break;
        case 'g':
            lv_obj_scroll_to_y(scroll_container, 0, LV_ANIM_ON);
            break;
        case 'G':
            lv_obj_scroll_to_y(scroll_container, LV_COORD_MAX, LV_ANIM_ON);
            break;
    }
}

/* Click handler for touch/mouse */
static void reader_click_cb(lv_event_t *e)
{
    (void)e;
    /* Could add tap zones for prev/next chapter */
}

static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    screen_manager_pop();
}

/* Load chapter list for navigation */
static void load_chapter_list(void)
{
    reader_chapter_count = 0;
    reader_chapter_index = 0;

    api_response_t *resp = api_get_chapters(reader_book_id);
    if (!resp || !resp->data) {
        api_response_free(resp);
        return;
    }

    cJSON *json = cJSON_Parse(resp->data);
    api_response_free(resp);
    if (!json) return;

    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (data && cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
        cJSON *first_book = cJSON_GetArrayItem(data, 0);
        cJSON *updated = cJSON_GetObjectItem(first_book, "updated");
        if (updated && cJSON_IsArray(updated)) {
            cJSON *ch;
            cJSON_ArrayForEach(ch, updated) {
                if (reader_chapter_count >= MAX_READER_CHAPTERS) break;

                cJSON *uid = cJSON_GetObjectItem(ch, "chapterUid");
                cJSON *title = cJSON_GetObjectItem(ch, "title");

                if (uid && title) {
                    reader_chapter_t *item = &reader_chapters[reader_chapter_count];
                    item->chapter_uid = uid->valueint;
                    snprintf(item->title, sizeof(item->title), "%s",
                             cJSON_IsString(title) ? title->valuestring : "");

                    /* Find current chapter index */
                    if (item->chapter_uid == reader_chapter_uid) {
                        reader_chapter_index = reader_chapter_count;
                    }
                    reader_chapter_count++;
                }
            }
        }
    }

    cJSON_Delete(json);
    printf("[READER] Loaded %d chapters, current index=%d\n",
           reader_chapter_count, reader_chapter_index);
}

/* Parse image file header to get pixel dimensions.
 * Supports JPEG (SOF marker) and PNG (IHDR chunk).
 * Returns 0 on success, -1 on failure. */
static int image_get_dimensions(const char *filepath, int *w, int *h)
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return -1;

    uint8_t magic[8];
    if (fread(magic, 1, 8, fp) != 8) { fclose(fp); return -1; }

    /* Check PNG: 89 50 4E 47 0D 0A 1A 0A */
    if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
        /* IHDR chunk: 4 bytes length + 4 bytes "IHDR" + 4 bytes width + 4 bytes height */
        uint8_t ihdr[16];
        if (fread(ihdr, 1, 16, fp) == 16) {
            *w = (ihdr[8] << 24) | (ihdr[9] << 16) | (ihdr[10] << 8) | ihdr[11];
            *h = (ihdr[12] << 24) | (ihdr[13] << 16) | (ihdr[14] << 8) | ihdr[15];
            fclose(fp);
            return (*w > 0 && *h > 0) ? 0 : -1;
        }
        fclose(fp);
        return -1;
    }

    /* Check JPEG: FF D8 */
    if (magic[0] != 0xFF || magic[1] != 0xD8) {
        fclose(fp);
        return -1;
    }

    /* Byte-by-byte scan for SOF marker (0xFF + C0-C3/C5-C7/C9-CB/CD-CF).
     * More robust than chain-following: handles any marker order or unknown markers. */
    int prev = -1;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (prev == 0xFF && ch >= 0xC0 && ch <= 0xCF) {
            /* Check if this is a real SOF (not C4/DHT, C8/JPG, CC/DAC) */
            if (ch == 0xC4 || ch == 0xC8 || ch == 0xCC) {
                prev = ch;
                continue;
            }
            /* Found SOF — read frame header: len(2) + precision(1) + height(2) + width(2) */
            uint8_t sof[7];
            if (fread(sof, 1, 7, fp) == 7) {
                *h = (sof[3] << 8) | sof[4];
                *w = (sof[5] << 8) | sof[6];
                fclose(fp);
                printf("[IMG] JPEG SOF 0x%02X: %dx%d\n", ch, *w, *h);
                return (*w > 0 && *h > 0) ? 0 : -1;
            }
            fclose(fp);
            return -1;
        }
        prev = ch;
    }

    fclose(fp);
    return -1;
}

/* Max decoded pixel area: 16MB / 4 bytes (ARGB8888) = ~4M pixels */
#define MAX_IMAGE_PIXELS    (4 * 1024 * 1024)

/* Load and display chapter content */
static void load_chapter_content(void)
{
    /* Update header */
    char header_text[300] = "";
    if (reader_chapter_index >= 0 && reader_chapter_index < reader_chapter_count) {
        snprintf(header_text, sizeof(header_text), "%s",
                 reader_chapters[reader_chapter_index].title);
    }
    lv_label_set_text(header_label, header_text);

    /* Update footer with navigation hints */
    char footer_text[128];
    snprintf(footer_text, sizeof(footer_text),
             "[%d/%d]  < > 换章  ^v 滚动  [v] 老板键  [q] 退出",
             reader_chapter_index + 1, reader_chapter_count);
    lv_label_set_text(footer_label, footer_text);

    /* Remove ALL old children from scroll container FIRST */
    uint32_t child_cnt = lv_obj_get_child_count(scroll_container);
    for (int32_t i = (int32_t)child_cnt - 1; i >= 0; i--) {
        lv_obj_del(lv_obj_get_child(scroll_container, i));
    }
    content_label = NULL;
    image_count = 0;

    /* Create fresh content label (prevents NULL deref on subsequent calls) */
    content_label = lv_label_create(scroll_container);
    lv_obj_set_width(content_label, lv_pct(100));
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
    if (chinese_font) lv_obj_set_style_text_font(content_label, chinese_font, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_line_space(content_label, 2, 0);
    lv_obj_set_style_bg_color(content_label, lv_color_hex(0xF7F4EF), 0);
    lv_obj_set_style_bg_opa(content_label, LV_OPA_COVER, 0);
    lv_label_set_text(content_label, "加载中...");

    /* Fetch and decrypt chapter content */
    char *html = NULL;
    char *style = NULL;
    int ret = api_fetch_chapter_content(reader_book_id, reader_chapter_uid,
                                        reader_book_format, &html, &style);
    if (ret != 0 || !html) {
        printf("[READER] Chapter %d: API failed (ret=%d, html=%p)\n",
               reader_chapter_uid, ret, (void *)html);
        lv_label_set_text(content_label, "章节内容加载失败");
        free(html);
        free(style);
        return;
    }
    printf("[READER] Chapter %d: HTML len=%zu\n", reader_chapter_uid, strlen(html));

    /* Debug: save raw HTML to file */
    {
        char dbg_path[128];
        snprintf(dbg_path, sizeof(dbg_path), "/tmp/weread_ch%d_html.txt", reader_chapter_uid);
        FILE *dbg = fopen(dbg_path, "w");
        if (dbg) { fwrite(html, 1, strlen(html), dbg); fclose(dbg); printf("[READER] Saved HTML -> %s\n", dbg_path); }
    }

    /* Re-cleanup: remove the loading label before rendering real content */
    child_cnt = lv_obj_get_child_count(scroll_container);
    for (int32_t i = (int32_t)child_cnt - 1; i >= 0; i--) {
        lv_obj_del(lv_obj_get_child(scroll_container, i));
    }
    content_label = NULL;

    /* Parse HTML to blocks */
    printf("[READER] HTML length: %zu\n", strlen(html));
    content_block_t *blocks = NULL;
    int block_count = 0;
    parse_html_to_blocks(html, &blocks, &block_count);
    printf("[READER] Parsed %d blocks\n", block_count);
    for (int i = 0; i < block_count && i < 20; i++) {
        printf("[READER]   block[%d] kind=%d text_len=%zu\n", i, blocks[i].kind,
               blocks[i].text ? strlen(blocks[i].text) : 0);
    }

    /* Interleaved rendering: walk blocks in order, insert text and images
     * at their natural positions instead of all-images-at-end. */
    #define TEXT_CHUNK_LINES 100
    #define MAX_DISPLAY_IMAGES 20

    /* Helper: add text as chunked labels to scroll_container */
    int seg_start = 0;  /* start of current text segment */

    for (int i = 0; i <= block_count; i++) {
        /* Flush text segment when we hit an image or reach the end */
        int is_image = (i < block_count && blocks[i].kind == BLOCK_IMAGE &&
                        blocks[i].text && blocks[i].text[0] &&
                        image_count < MAX_DISPLAY_IMAGES);
        int is_end = (i == block_count);

        if ((is_image || is_end) && i > seg_start) {
            /* Render text blocks [seg_start, i) to string */
            char *seg_text = render_blocks_range(blocks, seg_start, i, 72);
            if (seg_text && *seg_text) {
                char *text_ptr = seg_text;
                while (*text_ptr) {
                    char *chunk_end = text_ptr;
                    int lines = 0;
                    while (*chunk_end && lines < TEXT_CHUNK_LINES) {
                        if (*chunk_end == '\n') lines++;
                        chunk_end++;
                    }
                    size_t chunk_len = chunk_end - text_ptr;
                    char *chunk = malloc(chunk_len + 1);
                    memcpy(chunk, text_ptr, chunk_len);
                    chunk[chunk_len] = '\0';

                    lv_obj_t *lbl = lv_label_create(scroll_container);
                    lv_label_set_text(lbl, chunk);
                    lv_obj_set_width(lbl, lv_pct(100));
                    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
                    if (chinese_font) lv_obj_set_style_text_font(lbl, chinese_font, 0);
                    lv_obj_set_style_text_color(lbl, lv_color_hex(0x1A1A1A), 0);
                    lv_obj_set_style_text_line_space(lbl, 2, 0);
                    lv_obj_set_style_bg_color(lbl, lv_color_hex(0xF7F4EF), 0);
                    lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
                    if (!content_label) content_label = lbl;
                    free(chunk);
                    text_ptr = chunk_end;
                }
            }
            free(seg_text);
        }

        if (is_image) {
            /* Download and display this image at current position */
            const char *img_url = blocks[i].text;
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "/tmp/weread_img_%d_%d_%d.jpg",
                     reader_chapter_uid, image_count, (int)time(NULL) % 100000);

            if (api_download_image(img_url, filepath) == 0) {
                int img_w = 0, img_h = 0;
                if (image_get_dimensions(filepath, &img_w, &img_h) != 0) {
                    printf("[READER] Skipping unparseable image: %s\n", filepath);
                    remove(filepath);
                } else {
                    long pixels = (long)img_w * img_h;
                    if (pixels > MAX_IMAGE_PIXELS) {
                        printf("[READER] Skipping oversized image (%dx%d): %s\n",
                               img_w, img_h, filepath);
                        remove(filepath);
                    } else {
                        printf("[READER] Image %d at block[%d]: %dx%d\n",
                               image_count + 1, i, img_w, img_h);
                        lv_obj_t *img = lv_image_create(scroll_container);
                        lv_image_set_src(img, filepath);
                        lv_obj_set_width(img, lv_pct(100));
                        lv_obj_set_style_radius(img, 4, 0);
                        lv_obj_set_style_clip_corner(img, true, 0);
                        lv_obj_invalidate(img);
                        image_count++;
                    }
                }
            }
            seg_start = i + 1;  /* next text segment starts after this image */
        } else if (is_end) {
            break;
        }
    }

    /* Ensure content_label exists even if no text was rendered */
    if (!content_label) {
        content_label = lv_label_create(scroll_container);
        lv_obj_set_width(content_label, lv_pct(100));
        lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
        if (chinese_font) lv_obj_set_style_text_font(content_label, chinese_font, 0);
        lv_obj_set_style_text_color(content_label, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_color(content_label, lv_color_hex(0xF7F4EF), 0);
        lv_obj_set_style_bg_opa(content_label, LV_OPA_COVER, 0);
        lv_label_set_text(content_label, "");
    }

    content_blocks_free(blocks, block_count);

    /* Scroll to top */
    lv_obj_scroll_to_y(scroll_container, 0, LV_ANIM_OFF);

    free(html);
    free(style);

    printf("[READER] Chapter %d loaded (%d images)\n", reader_chapter_uid, image_count);
}

/* Switch to prev/next chapter */
static void switch_chapter(int delta)
{
    int new_index = reader_chapter_index + delta;
    if (new_index < 0) {
        printf("[READER] Already at first chapter\n");
        show_toast("已是第一章");
        return;
    }
    if (new_index >= reader_chapter_count) {
        printf("[READER] Already at last chapter\n");
        show_toast("已是最后一章");
        return;
    }

    reader_chapter_index = new_index;
    reader_chapter_uid = reader_chapters[new_index].chapter_uid;

    printf("[READER] Switching to chapter %d: %s\n",
           reader_chapter_uid, reader_chapters[new_index].title);

    load_chapter_content();
}

/* Toggle boss mode overlay */
static void toggle_boss_mode(void)
{
    boss_mode = !boss_mode;

    if (boss_mode) {
        /* Create boss overlay on the active screen */
        boss_overlay = lv_obj_create(lv_screen_active());
        lv_obj_set_size(boss_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(boss_overlay, lv_color_hex(0x0C0C0C), 0);
        lv_obj_set_style_bg_opa(boss_overlay, LV_OPA_COVER, 0);
        lv_obj_center(boss_overlay);

        /* Terminal-style text */
        lv_obj_t *term_text = lv_label_create(boss_overlay);
        lv_label_set_text(term_text, BOSS_FAKE_CONTENT);
        lv_obj_set_style_text_color(term_text, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_text_font(term_text, &lv_font_montserrat_14, 0);
        lv_obj_set_width(term_text, lv_pct(95));
        lv_label_set_long_mode(term_text, LV_LABEL_LONG_WRAP);
        lv_obj_align(term_text, LV_ALIGN_TOP_LEFT, 10, 10);

        lv_obj_t *hint = lv_label_create(boss_overlay);
        lv_label_set_text(hint, "[v] to return");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    } else {
        if (boss_overlay) {
            lv_obj_del(boss_overlay);
            boss_overlay = NULL;
        }
    }
}

/* Toggle fullscreen (hide header/footer) */
static void toggle_fullscreen(void)
{
    fullscreen_mode = !fullscreen_mode;

    if (fullscreen_mode) {
        lv_obj_add_flag(header_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(footer_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(scroll_container, lv_pct(98), lv_pct(98));
        lv_obj_align(scroll_container, LV_ALIGN_CENTER, 0, 0);
    } else {
        lv_obj_clear_flag(header_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(footer_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(scroll_container, lv_pct(95), lv_pct(82));
        lv_obj_align(scroll_container, LV_ALIGN_CENTER, 0, 10);
    }
}

lv_obj_t *screen_reader_create(const char *book_id, int chapter_uid, const char *book_format)
{
    strncpy(reader_book_id, book_id, sizeof(reader_book_id) - 1);
    reader_chapter_uid = chapter_uid;
    if (book_format) {
        strncpy(reader_book_format, book_format, sizeof(reader_book_format) - 1);
    }
    boss_mode = 0;
    fullscreen_mode = 0;

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF7F4EF), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* Enable keyboard events on screen */
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, reader_key_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(screen, reader_click_cb, LV_EVENT_CLICKED, NULL);

    /* Header */
    header_container = lv_obj_create(screen);
    lv_obj_set_size(header_container, lv_pct(100), 40);
    lv_obj_align(header_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header_container, lv_color_hex(0xC4612F), 0);
    lv_obj_set_style_border_width(header_container, 0, 0);
    lv_obj_set_flex_flow(header_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(header_container, 10, 0);
    lv_obj_set_style_pad_gap(header_container, 10, 0);

    lv_obj_t *back_btn = lv_button_create(header_container);
    lv_obj_set_size(back_btn, 50, 30);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "<");
    if (chinese_font) lv_obj_set_style_text_font(back_lbl, chinese_font, 0);
    lv_obj_center(back_lbl);

    header_label = lv_label_create(header_container);
    lv_label_set_text(header_label, "加载中...");
    if (chinese_font) lv_obj_set_style_text_font(header_label, chinese_font, 0);
    lv_obj_set_style_text_color(header_label, lv_color_white(), 0);
    lv_obj_set_width(header_label, lv_pct(80));
    lv_label_set_long_mode(header_label, LV_LABEL_LONG_DOT);

    /* Scrollable content container */
    scroll_container = lv_obj_create(screen);
    lv_obj_set_size(scroll_container, lv_pct(95), lv_pct(82));
    lv_obj_align(scroll_container, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(scroll_container, LV_DIR_VER);
    lv_obj_set_style_bg_color(scroll_container, lv_color_hex(0xF7F4EF), 0);
    lv_obj_set_style_bg_opa(scroll_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scroll_container, 0, 0);
    lv_obj_set_style_pad_all(scroll_container, 5, 0);
    lv_obj_set_style_pad_gap(scroll_container, 8, 0);

    /* Content text label (child of scroll container) */
    content_label = lv_label_create(scroll_container);
    lv_obj_set_width(content_label, lv_pct(100));
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
    if (chinese_font) lv_obj_set_style_text_font(content_label, chinese_font, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_line_space(content_label, 2, 0);

    /* Footer */
    footer_container = lv_obj_create(screen);
    lv_obj_set_size(footer_container, lv_pct(100), 30);
    lv_obj_align(footer_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(footer_container, lv_color_hex(0xE7E1D7), 0);
    lv_obj_set_style_border_width(footer_container, 0, 0);

    footer_label = lv_label_create(footer_container);
    lv_label_set_text(footer_label, "");
    lv_obj_set_style_text_color(footer_label, lv_color_hex(0x666666), 0);
    if (chinese_font) lv_obj_set_style_text_font(footer_label, chinese_font, 0);
    lv_obj_center(footer_label);

    /* Load chapter list first, then content */
    load_chapter_list();
    load_chapter_content();

    /* Add keyboard group */
    lv_group_t *group = lv_group_get_default();
    if (!group) {
        group = lv_group_create();
        lv_group_set_default(group);
    }
    /* Add the keyboard input device to the group */
    extern lv_indev_t *indev_keyboard;
    lv_indev_set_group(indev_keyboard, group);
    lv_group_add_obj(group, screen);

    return screen;
}
