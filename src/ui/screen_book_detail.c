/**
 * Book Detail Screen Implementation
 *
 * Shows book info (title, author, category), reading progress, and chapter list.
 */

#include "screen_book_detail.h"
#include "screen_manager.h"
#include "../api/api_client.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern lv_font_t *chinese_font;

/* Chapter data */
#define MAX_CHAPTERS 1000
typedef struct {
    int chapter_uid;
    char title[256];
    int level;
    int pay_status;
} chapter_item_t;

static chapter_item_t chapters[MAX_CHAPTERS];
static int chapter_count = 0;
static char current_book_id[64] = {0};
static char book_format[16] = "epub";
static int progress_chapter_uid = 0;
static int progress_pct = 0;

static lv_obj_t *screen = NULL;

/* Book detail UI widgets */
static lv_obj_t *title_label = NULL;
static lv_obj_t *meta_label = NULL;
static lv_obj_t *progress_label = NULL;
static lv_obj_t *chapter_list_widget = NULL;
static lv_obj_t *read_btn = NULL;

/* Chapter click handler */
static void chapter_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    chapter_item_t *ch = (chapter_item_t *)lv_event_get_user_data(e);
    if (ch) {
        printf("[DETAIL] Opening chapter: %s (uid=%d)\n", ch->title, ch->chapter_uid);
        screen_manager_push_reader(current_book_id, ch->chapter_uid, book_format);
    }
}

/* Read/Continue button handler */
static void read_btn_cb(lv_event_t *e)
{
    (void)e;
    int start_uid = progress_chapter_uid;
    if (start_uid == 0 && chapter_count > 0) {
        start_uid = chapters[0].chapter_uid;
    }
    if (start_uid > 0) {
        screen_manager_push_reader(current_book_id, start_uid, book_format);
    }
}

/* Back button */
static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    screen_manager_pop();
}

static void load_book_detail(const char *book_id)
{
    chapter_count = 0;
    progress_chapter_uid = 0;
    progress_pct = 0;
    strncpy(book_format, "epub", sizeof(book_format));  /* Reset to default */

    /* Load book info */
    api_response_t *info_resp = api_get_book_info(book_id);
    if (info_resp && info_resp->data) {
        cJSON *json = cJSON_Parse(info_resp->data);
        if (json) {
            cJSON *title = cJSON_GetObjectItem(json, "title");
            cJSON *author = cJSON_GetObjectItem(json, "author");
            cJSON *translator = cJSON_GetObjectItem(json, "translator");
            cJSON *category = cJSON_GetObjectItem(json, "categoryName");
            cJSON *fmt = cJSON_GetObjectItem(json, "format");

            if (title && cJSON_IsString(title)) {
                lv_label_set_text(title_label, title->valuestring);
            }

            char meta[512] = "";
            if (author && cJSON_IsString(author) && strlen(author->valuestring) > 0) {
                strcat(meta, author->valuestring);
            }
            if (translator && cJSON_IsString(translator) && strlen(translator->valuestring) > 0) {
                if (meta[0]) strcat(meta, " · 译: ");
                else strcat(meta, "译: ");
                strcat(meta, translator->valuestring);
            }
            if (category && cJSON_IsString(category) && strlen(category->valuestring) > 0) {
                if (meta[0]) strcat(meta, " · ");
                strcat(meta, category->valuestring);
            }
            if (meta[0]) {
                lv_label_set_text(meta_label, meta);
            }

            if (fmt && cJSON_IsString(fmt)) {
                strncpy(book_format, fmt->valuestring, sizeof(book_format) - 1);
                printf("[DETAIL] Book format: %s\n", book_format);
            } else {
                printf("[DETAIL] Book format: %s (default, fmt=%s)\n", book_format,
                       fmt ? (cJSON_IsNumber(fmt) ? "number" : "other") : "null");
            }

            cJSON_Delete(json);
        }
    }
    api_response_free(info_resp);

    /* Load chapters */
    api_response_t *ch_resp = api_get_chapters(book_id);
    if (ch_resp && ch_resp->data) {
        cJSON *json = cJSON_Parse(ch_resp->data);
        if (json) {
            cJSON *data = cJSON_GetObjectItem(json, "data");
            if (data && cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
                cJSON *first_book = cJSON_GetArrayItem(data, 0);
                cJSON *updated = cJSON_GetObjectItem(first_book, "updated");
                if (updated && cJSON_IsArray(updated)) {
                    cJSON *ch;
                    cJSON_ArrayForEach(ch, updated) {
                        if (chapter_count >= MAX_CHAPTERS) break;

                        cJSON *uid = cJSON_GetObjectItem(ch, "chapterUid");
                        cJSON *ch_title = cJSON_GetObjectItem(ch, "title");
                        cJSON *level = cJSON_GetObjectItem(ch, "level");
                        cJSON *pay = cJSON_GetObjectItem(ch, "payStatus");

                        if (uid && ch_title) {
                            chapter_item_t *item = &chapters[chapter_count];
                            item->chapter_uid = uid->valueint;
                            snprintf(item->title, sizeof(item->title), "%s",
                                     cJSON_IsString(ch_title) ? ch_title->valuestring : "");
                            item->level = level ? level->valueint : 1;
                            item->pay_status = pay ? pay->valueint : 0;

                            /* Create chapter list item */
                            lv_obj_t *btn = lv_obj_create(chapter_list_widget);
                            lv_obj_set_width(btn, lv_pct(100));
                            lv_obj_set_height(btn, 44);
                            lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
                            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
                            lv_obj_set_style_border_width(btn, 0, 0);
                            lv_obj_set_style_pad_all(btn, 4, 0);
                            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
                            lv_obj_add_event_cb(btn, chapter_click_cb, LV_EVENT_CLICKED, item);

                            /* Build label with indentation and lock icon */
                            char label_text[300] = "";
                            for (int i = 1; i < item->level; i++) strcat(label_text, "  ");
                            if (item->pay_status) strcat(label_text, "🔒 ");
                            strncat(label_text, item->title, sizeof(label_text) - strlen(label_text) - 1);

                            lv_obj_t *lbl = lv_label_create(btn);
                            lv_label_set_text(lbl, label_text);
                            if (chinese_font) lv_obj_set_style_text_font(lbl, chinese_font, 0);
                            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
                            lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
                            lv_obj_set_width(lbl, lv_pct(95));

                            chapter_count++;
                        }
                    }
                }
            }
            cJSON_Delete(json);
        }
    }
    api_response_free(ch_resp);

    /* Load reading progress */
    api_response_t *prog_resp = api_get_progress(book_id);
    if (prog_resp && prog_resp->data) {
        cJSON *json = cJSON_Parse(prog_resp->data);
        if (json) {
            cJSON *book_obj = cJSON_GetObjectItem(json, "book");
            if (book_obj) {
                cJSON *prog = cJSON_GetObjectItem(book_obj, "progress");
                cJSON *ch_uid = cJSON_GetObjectItem(book_obj, "chapterUid");

                if (prog) progress_pct = prog->valueint;
                if (ch_uid) progress_chapter_uid = ch_uid->valueint;

                char prog_text[128];
                snprintf(prog_text, sizeof(prog_text), "阅读进度: %d%%", progress_pct);
                lv_label_set_text(progress_label, prog_text);

                /* Update read button text */
                if (progress_pct > 0) {
                    lv_obj_t *lbl = lv_obj_get_child(read_btn, 0);
                    if (lbl) lv_label_set_text(lbl, "继续阅读");
                }
            }
            cJSON_Delete(json);
        }
    }
    api_response_free(prog_resp);

    /* Update chapter header */
    printf("[DETAIL] Loaded %d chapters for book %s\n", chapter_count, book_id);
}

lv_obj_t *screen_book_detail_create(const char *book_id)
{
    strncpy(current_book_id, book_id, sizeof(current_book_id) - 1);
    chapter_count = 0;

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF7F4EF), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    /* Header */
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, lv_pct(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(header, 10, 0);
    lv_obj_set_style_pad_gap(header, 10, 0);

    lv_obj_t *back_btn = lv_button_create(header);
    lv_obj_set_size(back_btn, 60, 36);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "← 返回");
    if (chinese_font) lv_obj_set_style_text_font(back_lbl, chinese_font, 0);
    lv_obj_center(back_lbl);

    read_btn = lv_button_create(header);
    lv_obj_set_size(read_btn, 100, 36);
    lv_obj_set_style_bg_color(read_btn, lv_color_hex(0x07C160), 0);
    lv_obj_set_style_bg_opa(read_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(read_btn, 0, 0);
    lv_obj_add_event_cb(read_btn, read_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *read_lbl = lv_label_create(read_btn);
    lv_label_set_text(read_lbl, "开始阅读");
    if (chinese_font) lv_obj_set_style_text_font(read_lbl, chinese_font, 0);
    lv_obj_center(read_lbl);

    /* Book info area */
    lv_obj_t *info_area = lv_obj_create(screen);
    lv_obj_set_size(info_area, lv_pct(95), 120);
    lv_obj_align(info_area, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_color(info_area, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(info_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(info_area, 0, 0);

    title_label = lv_label_create(info_area);
    lv_label_set_text(title_label, "加载中...");
    if (chinese_font) lv_obj_set_style_text_font(title_label, chinese_font, 0);
    lv_obj_set_width(title_label, lv_pct(95));
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 10);

    meta_label = lv_label_create(info_area);
    lv_label_set_text(meta_label, "");
    if (chinese_font) lv_obj_set_style_text_font(meta_label, chinese_font, 0);
    lv_obj_set_style_text_color(meta_label, lv_color_hex(0x666666), 0);
    lv_obj_set_width(meta_label, lv_pct(95));
    lv_label_set_long_mode(meta_label, LV_LABEL_LONG_DOT);
    lv_obj_align(meta_label, LV_ALIGN_TOP_LEFT, 10, 45);

    progress_label = lv_label_create(info_area);
    lv_label_set_text(progress_label, "");
    if (chinese_font) lv_obj_set_style_text_font(progress_label, chinese_font, 0);
    lv_obj_set_style_text_color(progress_label, lv_color_hex(0xC4612F), 0);
    lv_obj_align(progress_label, LV_ALIGN_TOP_LEFT, 10, 75);

    /* Chapter list header */
    lv_obj_t *ch_header = lv_label_create(screen);
    lv_label_set_text(ch_header, "目录");
    if (chinese_font) lv_obj_set_style_text_font(ch_header, chinese_font, 0);
    lv_obj_align(ch_header, LV_ALIGN_TOP_LEFT, 20, 185);

    /* Chapter list */
    chapter_list_widget = lv_obj_create(screen);
    lv_obj_set_size(chapter_list_widget, lv_pct(95), lv_pct(62));
    lv_obj_align(chapter_list_widget, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(chapter_list_widget, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(chapter_list_widget, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chapter_list_widget, 0, 0);
    lv_obj_set_flex_flow(chapter_list_widget, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(chapter_list_widget, LV_DIR_VER);
    lv_obj_set_style_pad_all(chapter_list_widget, 0, 0);
    lv_obj_set_style_pad_gap(chapter_list_widget, 2, 0);

    /* Load data */
    load_book_detail(book_id);

    return screen;
}
