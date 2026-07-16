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

extern lv_font_t *chinese_font;

/* Reader state */
static lv_obj_t *screen = NULL;
static lv_obj_t *header_label = NULL;
static lv_obj_t *content_label = NULL;
static lv_obj_t *footer_label = NULL;
static lv_obj_t *boss_overlay = NULL;
static lv_obj_t *header_container = NULL;
static lv_obj_t *footer_container = NULL;

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
            switch_chapter(1);
            break;
        case LV_KEY_LEFT:
        case ',':
            switch_chapter(-1);
            break;
        case LV_KEY_DOWN:
            lv_obj_scroll_by(content_label, 0, 150, LV_ANIM_ON);
            break;
        case LV_KEY_UP:
            lv_obj_scroll_by(content_label, 0, -150, LV_ANIM_ON);
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
            lv_obj_scroll_to_y(content_label, 0, LV_ANIM_ON);
            break;
        case 'G':
            lv_obj_scroll_to_y(content_label, LV_COORD_MAX, LV_ANIM_ON);
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

/* Load and display chapter content */
static void load_chapter_content(void)
{
    /* Show loading message */
    lv_label_set_text(content_label, "加载中...");

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

    /* Fetch and decrypt chapter content */
    char *html = NULL;
    char *style = NULL;
    int ret = api_fetch_chapter_content(reader_book_id, reader_chapter_uid,
                                        reader_book_format, &html, &style);
    if (ret != 0 || !html) {
        lv_label_set_text(content_label, "章节内容加载失败");
        free(html);
        free(style);
        return;
    }

    /* Parse HTML and render to display text */
    printf("[READER] HTML length: %zu\n", strlen(html));
    char *display_text = html_to_display_text(html, 72);
    if (display_text) {
        size_t dlen = strlen(display_text);
        printf("[READER] Display text length: %zu, first 200 chars: %.200s\n", dlen, display_text);
        lv_label_set_text(content_label, display_text);
        free(display_text);
    } else {
        printf("[READER] html_to_display_text returned NULL\n");
        lv_label_set_text(content_label, "内容渲染失败");
    }

    /* Scroll to top */
    lv_obj_scroll_to_y(content_label, 0, LV_ANIM_OFF);

    free(html);
    free(style);

    printf("[READER] Chapter %d loaded\n", reader_chapter_uid);
}

/* Switch to prev/next chapter */
static void switch_chapter(int delta)
{
    int new_index = reader_chapter_index + delta;
    if (new_index < 0 || new_index >= reader_chapter_count) {
        printf("[READER] No more chapters in that direction\n");
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
        /* Expand content to full screen */
        lv_obj_set_size(content_label, lv_pct(98), lv_pct(98));
        lv_obj_align(content_label, LV_ALIGN_CENTER, 0, 0);
    } else {
        lv_obj_clear_flag(header_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(footer_container, LV_OBJ_FLAG_HIDDEN);
        /* Restore normal layout */
        lv_obj_set_size(content_label, lv_pct(95), lv_pct(82));
        lv_obj_align(content_label, LV_ALIGN_CENTER, 0, 0);
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

    /* Content area (scrollable) */
    content_label = lv_label_create(screen);
    lv_obj_set_size(content_label, lv_pct(95), lv_pct(82));
    lv_obj_align(content_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
    if (chinese_font) lv_obj_set_style_text_font(content_label, chinese_font, 0);
    lv_obj_set_style_text_color(content_label, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_scroll_dir(content_label, LV_DIR_VER);
    lv_obj_set_style_pad_all(content_label, 5, 0);
    lv_obj_set_style_text_line_space(content_label, 2, 0);
    lv_obj_align(content_label, LV_ALIGN_CENTER, 0, 10);

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
