/**
 * WeRead Shelf Screen Implementation
 */

#include "screen_shelf.h"
#include "screen_manager.h"
#include "../api/api_client.h"
#include "lvgl.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern lv_font_t *chinese_font;
extern lv_font_t *chinese_font_large;

static lv_obj_t *screen = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *book_list = NULL;
static lv_obj_t *loading_spinner = NULL;

/* Book item structure */
typedef struct {
    char book_id[64];
    char title[256];
    char author[128];
    int progress;
    int finish_reading;     /* 0 or timestamp */
    int64_t read_update_time;
} book_item_t;

/* Array to store books */
#define MAX_BOOKS 100
static book_item_t books[MAX_BOOKS];
static int book_count = 0;

/* Event handler for book item click - navigate to book detail */
static void book_item_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        book_item_t *book = (book_item_t *)lv_event_get_user_data(e);
        if (book) {
            printf("[SHELF] Opening book: %s (%s)\n", book->title, book->book_id);
            screen_manager_push_book_detail(book->book_id);
        }
    }
}

/* Discover button handler */
static void discover_btn_cb(lv_event_t *e)
{
    (void)e;
    screen_manager_push(SCREEN_DISCOVER);
}

/* Logout button handler */
static void logout_btn_cb(lv_event_t *e)
{
    (void)e;
    printf("[SHELF] User logging out...\n");
    api_clear_cookies();
    /* Go back to welcome screen (replace current shelf) */
    screen_manager_replace(SCREEN_WELCOME);
}

lv_obj_t *screen_shelf_create(void)
{
    book_count = 0;

    /* Create screen */
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1F2421), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    /* Header */
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, lv_pct(100), 55);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1F2421), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(header, 20, 0);
    lv_obj_set_style_pad_right(header, 20, 0);

    title_label = lv_label_create(header);
    lv_label_set_text(title_label, "我的书架");
    if (chinese_font_large) lv_obj_set_style_text_font(title_label, chinese_font_large, 0);
    else if (chinese_font) lv_obj_set_style_text_font(title_label, chinese_font, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);

    /* Discover button */
    lv_obj_t *discover_btn = lv_button_create(header);
    lv_obj_set_size(discover_btn, 80, 36);
    lv_obj_set_style_bg_color(discover_btn, lv_color_hex(0x07C160), 0);
    lv_obj_set_style_bg_opa(discover_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(discover_btn, 0, 0);
    lv_obj_add_event_cb(discover_btn, discover_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *discover_lbl = lv_label_create(discover_btn);
    lv_label_set_text(discover_lbl, "发现");
    if (chinese_font) lv_obj_set_style_text_font(discover_lbl, chinese_font, 0);
    lv_obj_center(discover_lbl);

    /* Logout button */
    lv_obj_t *logout_btn = lv_button_create(header);
    lv_obj_set_size(logout_btn, 80, 36);
    lv_obj_set_style_bg_color(logout_btn, lv_color_hex(0xE64340), 0);
    lv_obj_set_style_bg_opa(logout_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(logout_btn, 0, 0);
    lv_obj_add_event_cb(logout_btn, logout_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *logout_lbl = lv_label_create(logout_btn);
    lv_label_set_text(logout_lbl, "登出");
    if (chinese_font) lv_obj_set_style_text_font(logout_lbl, chinese_font, 0);
    lv_obj_center(logout_lbl);

    /* Create scrollable list container */
    book_list = lv_obj_create(screen);
    lv_obj_set_size(book_list, lv_pct(95), lv_pct(88));
    lv_obj_align(book_list, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(book_list, lv_color_hex(0x2A2D2A), 0);
    lv_obj_set_style_bg_opa(book_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(book_list, 0, 0);
    lv_obj_set_flex_flow(book_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(book_list, LV_DIR_VER);
    lv_obj_set_style_pad_all(book_list, 5, 0);
    lv_obj_set_style_pad_gap(book_list, 4, 0);

    /* Loading spinner (hidden initially) */
    loading_spinner = lv_spinner_create(book_list);
    lv_obj_set_size(loading_spinner, 50, 50);
    lv_obj_center(loading_spinner);
    lv_obj_add_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);

    return screen;
}

/* Sort books by readUpdateTime descending (newest first) */
static int compare_books_by_time(const void *a, const void *b)
{
    const book_item_t *ba = (const book_item_t *)a;
    const book_item_t *bb = (const book_item_t *)b;
    if (bb->read_update_time > ba->read_update_time) return 1;
    if (bb->read_update_time < ba->read_update_time) return -1;
    return 0;
}

/* Create UI for a single book item */
static void create_book_item_ui(book_item_t *book)
{
    lv_obj_t *book_item = lv_obj_create(book_list);
    lv_obj_set_width(book_item, lv_pct(100));
    lv_obj_set_height(book_item, 55);
    lv_obj_set_style_bg_color(book_item, lv_color_hex(0x2A2D2A), 0);
    lv_obj_set_style_bg_opa(book_item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(book_item, 0, 0);
    lv_obj_set_style_pad_all(book_item, 6, 0);
    lv_obj_add_flag(book_item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(book_item, book_item_clicked, LV_EVENT_CLICKED, book);

    /* Book title */
    lv_obj_t *ttl_lbl = lv_label_create(book_item);
    lv_label_set_text(ttl_lbl, book->title);
    if (chinese_font) lv_obj_set_style_text_font(ttl_lbl, chinese_font, 0);
    lv_obj_set_style_text_color(ttl_lbl, lv_color_white(), 0);
    lv_obj_align(ttl_lbl, LV_ALIGN_TOP_LEFT, 5, 3);
    lv_label_set_long_mode(ttl_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ttl_lbl, lv_pct(70));

    /* Author */
    if (book->author[0] != '\0') {
        lv_obj_t *author_label = lv_label_create(book_item);
        lv_label_set_text(author_label, book->author);
        if (chinese_font) lv_obj_set_style_text_font(author_label, chinese_font, 0);
        lv_obj_set_style_text_color(author_label, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(author_label, LV_ALIGN_BOTTOM_LEFT, 5, -3);
    }

    /* Status badge */
    char status_text[64] = "";
    if (book->finish_reading) {
        snprintf(status_text, sizeof(status_text), "✓ 已读完");
    } else if (book->read_update_time > 0) {
        time_t t = (time_t)book->read_update_time;
        struct tm *tm_info = localtime(&t);
        if (tm_info) {
            snprintf(status_text, sizeof(status_text), "最后阅读: %02d-%02d",
                     tm_info->tm_mon + 1, tm_info->tm_mday);
        }
    }
    if (status_text[0]) {
        lv_obj_t *status_lbl = lv_label_create(book_item);
        lv_label_set_text(status_lbl, status_text);
        if (chinese_font) lv_obj_set_style_text_font(status_lbl, chinese_font, 0);
        lv_obj_set_style_text_color(status_lbl,
            book->finish_reading ? lv_color_hex(0x07C160) : lv_color_hex(0x999999), 0);
        lv_obj_align(status_lbl, LV_ALIGN_TOP_RIGHT, -5, 3);
    }

    /* Progress bar */
    if (book->progress > 0) {
        lv_obj_t *progress_bar = lv_bar_create(book_item);
        lv_obj_set_size(progress_bar, 120, 6);
        lv_bar_set_value(progress_bar, book->progress, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x3A3D3A), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(progress_bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x07C160), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_align(progress_bar, LV_ALIGN_BOTTOM_RIGHT, -5, -3);

        char progress_text[16];
        snprintf(progress_text, sizeof(progress_text), "%d%%", book->progress);
        lv_obj_t *prog_lbl = lv_label_create(book_item);
        lv_label_set_text(prog_lbl, progress_text);
        lv_obj_set_style_text_color(prog_lbl, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(prog_lbl, LV_ALIGN_BOTTOM_RIGHT, -135, -2);
    }
}

void screen_shelf_load_books(void)
{
    /* Show loading spinner */
    lv_obj_clear_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);

    /* Call API to get shelf data */
    api_response_t *response = api_get_shelf();

    if (!response || response->status_code != 200 || !response->data) {
        printf("[SHELF] Failed to get shelf data (status=%d)\n",
               response ? response->status_code : -1);
        api_response_free(response);
        lv_obj_add_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);

        /* Show error message */
        lv_obj_t *error_label = lv_label_create(book_list);
        lv_label_set_text(error_label, "加载失败，请重试");
        if (chinese_font) lv_obj_set_style_text_font(error_label, chinese_font, 0);
        lv_obj_set_style_text_color(error_label, lv_color_hex(0xFF6666), 0);
        lv_obj_center(error_label);
        return;
    }

    printf("[SHELF] Response: %.500s\n", response->data);

    /* Check for login expiration */
    cJSON *check_json = cJSON_Parse(response->data);
    if (check_json) {
        cJSON *err_code = cJSON_GetObjectItem(check_json, "errCode");
        if (err_code && err_code->valueint == -2012) {
            printf("[SHELF] Login expired, clearing cookies and redirecting to login\n");
            cJSON_Delete(check_json);
            api_response_free(response);
            api_clear_cookies();
            screen_manager_replace(SCREEN_LOGIN);
            return;
        }
        cJSON_Delete(check_json);
    }

    /* Parse JSON response */
    cJSON *json = cJSON_Parse(response->data);
    api_response_free(response);

    if (!json) {
        printf("[SHELF] Failed to parse JSON\n");
        lv_obj_add_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Extract books array */
    cJSON *books_array = cJSON_GetObjectItem(json, "books");
    if (!books_array || !cJSON_IsArray(books_array)) {
        printf("[SHELF] No books array in response\n");
        cJSON_Delete(json);
        lv_obj_add_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Clear previous books */
    book_count = 0;
    lv_obj_clean(book_list);

    /* Phase 1: Parse all books into array */
    cJSON *book_json = NULL;
    cJSON_ArrayForEach(book_json, books_array) {
        if (book_count >= MAX_BOOKS) break;

        book_item_t *book = &books[book_count];

        cJSON *book_id = cJSON_GetObjectItem(book_json, "bookId");
        cJSON *title = cJSON_GetObjectItem(book_json, "title");
        cJSON *author = cJSON_GetObjectItem(book_json, "author");
        cJSON *progress_obj = cJSON_GetObjectItem(book_json, "readingProgress");
        cJSON *finish_obj = cJSON_GetObjectItem(book_json, "finishReading");
        cJSON *read_time_obj = cJSON_GetObjectItem(book_json, "readUpdateTime");

        if (!book_id || !title) continue;

        snprintf(book->book_id, sizeof(book->book_id), "%s",
                 cJSON_IsString(book_id) ? book_id->valuestring : "");
        snprintf(book->title, sizeof(book->title), "%s",
                 cJSON_IsString(title) ? title->valuestring : "未知书名");

        if (author && cJSON_IsString(author)) {
            snprintf(book->author, sizeof(book->author), "%s", author->valuestring);
        } else {
            book->author[0] = '\0';
        }

        book->progress = progress_obj ? progress_obj->valueint : 0;
        book->finish_reading = finish_obj ? finish_obj->valueint : 0;
        book->read_update_time = read_time_obj ? (int64_t)read_time_obj->valuedouble : 0;

        book_count++;
    }

    /* Phase 2: Sort by readUpdateTime descending (newest first) */
    if (book_count > 1) {
        qsort(books, book_count, sizeof(book_item_t), compare_books_by_time);
    }

    /* Phase 3: Create UI elements in sorted order */
    for (int i = 0; i < book_count; i++) {
        create_book_item_ui(&books[i]);
    }

    cJSON_Delete(json);
    lv_obj_add_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);

    printf("[SHELF] Loaded %d books (sorted by recent)\n", book_count);
}

void screen_shelf_set_book_callback(void (*callback)(const char *book_id))
{
    (void)callback;  /* No longer used - navigation via screen_manager */
}

void screen_shelf_cleanup(void)
{
    if (screen) {
        lv_obj_del(screen);
        screen = NULL;
    }

    book_list = NULL;
    title_label = NULL;
    loading_spinner = NULL;
    book_count = 0;
}
