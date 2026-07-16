/**
 * Discover Screen Implementation
 *
 * Shows categories and recommended books with tabbed navigation.
 */

#include "screen_discover.h"
#include "screen_manager.h"
#include "../api/api_client.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern lv_font_t *chinese_font;

static lv_obj_t *screen = NULL;
static lv_obj_t *category_list = NULL;
static lv_obj_t *book_panel = NULL;
static lv_obj_t *book_list_widget = NULL;
static lv_obj_t *tab_btns = NULL;
static lv_obj_t *category_container = NULL;
static lv_obj_t *recommend_container = NULL;

/* Category data */
#define MAX_CATEGORIES 64
typedef struct {
    char id[64];
    char name[128];
    int book_count;
} category_item_t;
static category_item_t categories[MAX_CATEGORIES];
static int category_count = 0;

/* Forward declarations */
static void load_categories(void);
static void load_recommend_books(void);
static void load_category_books(const char *category_id);

/* Tab switch handler */
static void tab_switch_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int tab_idx = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (tab_idx == 0) {
        /* Categories tab */
        lv_obj_clear_flag(category_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(recommend_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        /* Recommend tab */
        lv_obj_add_flag(category_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(recommend_container, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Category item click */
static void category_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    category_item_t *cat = (category_item_t *)lv_event_get_user_data(e);
    if (cat) {
        printf("[DISCOVER] Selected category: %s (%s)\n", cat->name, cat->id);
        load_category_books(cat->id);
    }
}

/* Book item click (from discover) */
typedef struct {
    char book_id[64];
    char title[128];
} discover_book_t;

#define MAX_DISCOVER_BOOKS 100
static discover_book_t discover_books[MAX_DISCOVER_BOOKS];
static int discover_book_count = 0;

static void discover_book_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    discover_book_t *book = (discover_book_t *)lv_event_get_user_data(e);
    if (book) {
        printf("[DISCOVER] Opening book: %s\n", book->title);
        screen_manager_push_book_detail(book->book_id);
    }
}

/* Back button */
static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    screen_manager_pop();
}

static void add_book_to_panel(lv_obj_t *parent, const char *book_id, const char *title, const char *author)
{
    if (discover_book_count >= MAX_DISCOVER_BOOKS) return;

    discover_book_t *book = &discover_books[discover_book_count];
    strncpy(book->book_id, book_id, sizeof(book->book_id) - 1);
    strncpy(book->title, title, sizeof(book->title) - 1);
    discover_book_count++;

    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_set_width(item, lv_pct(100));
    lv_obj_set_height(item, 60);
    lv_obj_set_style_bg_color(item, lv_color_white(), 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xE7E1D7), 0);
    lv_obj_set_style_pad_all(item, 8, 0);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, discover_book_click_cb, LV_EVENT_CLICKED, book);

    lv_obj_t *title_lbl = lv_label_create(item);
    lv_label_set_text(title_lbl, title);
    if (chinese_font) lv_obj_set_style_text_font(title_lbl, chinese_font, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    if (author && author[0]) {
        lv_obj_t *author_lbl = lv_label_create(item);
        lv_label_set_text(author_lbl, author);
        if (chinese_font) lv_obj_set_style_text_font(author_lbl, chinese_font, 0);
        lv_obj_set_style_text_color(author_lbl, lv_color_hex(0x999999), 0);
        lv_obj_align(author_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
}

static void load_categories(void)
{
    api_response_t *resp = api_get_categories();
    if (!resp || !resp->data) {
        printf("[DISCOVER] Failed to load categories (status=%d)\n",
               resp ? resp->status_code : -1);
        api_response_free(resp);
        return;
    }

    printf("[DISCOVER] Categories response: %.500s\n", resp->data);

    cJSON *json = cJSON_Parse(resp->data);
    api_response_free(resp);
    if (!json) return;

    /* Response structure: {"data": [{"name":"排行榜","categories":[{"CategoryId":"...","title":"...","totalCount":N}, ...]}, ...]} */
    cJSON *data_arr = cJSON_GetObjectItem(json, "data");
    if (data_arr && cJSON_IsArray(data_arr)) {
        category_count = 0;
        cJSON *group;
        cJSON_ArrayForEach(group, data_arr) {
            /* Each group may have a "name" for the section header */
            cJSON *group_name = cJSON_GetObjectItem(group, "name");

            /* Add section header if group has a name */
            if (group_name && cJSON_IsString(group_name) && category_list) {
                lv_obj_t *hdr = lv_label_create(category_list);
                lv_label_set_text(hdr, group_name->valuestring);
                if (chinese_font) lv_obj_set_style_text_font(hdr, chinese_font, 0);
                lv_obj_set_style_text_color(hdr, lv_color_hex(0x666666), 0);
                lv_obj_set_style_pad_top(hdr, 8, 0);
            }

            cJSON *cats = cJSON_GetObjectItem(group, "categories");
            if (!cats || !cJSON_IsArray(cats)) continue;

            cJSON *cat;
            cJSON_ArrayForEach(cat, cats) {
                if (category_count >= MAX_CATEGORIES) break;

                /* Fields: CategoryId (capital C), title, totalCount */
                cJSON *cid = cJSON_GetObjectItem(cat, "CategoryId");
                if (!cid) cid = cJSON_GetObjectItem(cat, "categoryId");
                cJSON *name = cJSON_GetObjectItem(cat, "title");
                if (!name) name = cJSON_GetObjectItem(cat, "name");
                cJSON *count = cJSON_GetObjectItem(cat, "totalCount");
                if (!count) count = cJSON_GetObjectItem(cat, "bookCount");

                if (cid && name) {
                    category_item_t *item = &categories[category_count];
                    snprintf(item->id, sizeof(item->id), "%s",
                             cJSON_IsString(cid) ? cid->valuestring : "");
                    snprintf(item->name, sizeof(item->name), "%s",
                             cJSON_IsString(name) ? name->valuestring : "");
                    item->book_count = count ? count->valueint : 0;

                    /* Create category button in list */
                    lv_obj_t *btn = lv_obj_create(category_list);
                    lv_obj_set_width(btn, lv_pct(100));
                    lv_obj_set_height(btn, 40);
                    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
                    lv_obj_set_style_border_width(btn, 1, 0);
                    lv_obj_set_style_border_color(btn, lv_color_hex(0xE7E1D7), 0);
                    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_add_event_cb(btn, category_click_cb, LV_EVENT_CLICKED, item);

                    char label_text[256];
                    snprintf(label_text, sizeof(label_text), "%s  (%d 本)", item->name, item->book_count);
                    lv_obj_t *lbl = lv_label_create(btn);
                    lv_label_set_text(lbl, label_text);
                    if (chinese_font) lv_obj_set_style_text_font(lbl, chinese_font, 0);
                    lv_obj_center(lbl);

                    category_count++;
                }
            }
        }
    }

    cJSON_Delete(json);
    printf("[DISCOVER] Loaded %d categories\n", category_count);
}

static void load_category_books(const char *category_id)
{
    /* Clear book panel */
    lv_obj_clean(book_list_widget);
    discover_book_count = 0;

    /* Show loading */
    lv_obj_t *loading = lv_label_create(book_list_widget);
    lv_label_set_text(loading, "加载中...");
    if (chinese_font) lv_obj_set_style_text_font(loading, chinese_font, 0);

    api_response_t *resp = api_get_category_books(category_id);
    lv_obj_clean(book_list_widget);

    if (!resp || !resp->data) {
        api_response_free(resp);
        lv_obj_t *err = lv_label_create(book_list_widget);
        lv_label_set_text(err, "加载失败");
        if (chinese_font) lv_obj_set_style_text_font(err, chinese_font, 0);
        return;
    }

    cJSON *json = cJSON_Parse(resp->data);
    api_response_free(resp);
    if (!json) return;

    cJSON *books = cJSON_GetObjectItem(json, "books");
    if (books && cJSON_IsArray(books)) {
        cJSON *book;
        cJSON_ArrayForEach(book, books) {
            cJSON *info = cJSON_GetObjectItem(book, "bookInfo");
            if (!info) info = book;

            cJSON *bid = cJSON_GetObjectItem(info, "bookId");
            if (!bid) bid = cJSON_GetObjectItem(book, "bookId");
            cJSON *title = cJSON_GetObjectItem(info, "title");
            cJSON *author = cJSON_GetObjectItem(info, "author");

            if (bid && title) {
                add_book_to_panel(book_list_widget,
                                  cJSON_IsString(bid) ? bid->valuestring : "",
                                  cJSON_IsString(title) ? title->valuestring : "未知",
                                  cJSON_IsString(author) ? author->valuestring : "");
            }
        }
    }

    cJSON_Delete(json);
}

static void load_recommend_books(void)
{
    api_response_t *resp = api_get_recommend_books();
    if (!resp || !resp->data) {
        printf("[DISCOVER] Failed to load recommendations\n");
        api_response_free(resp);
        return;
    }

    cJSON *json = cJSON_Parse(resp->data);
    api_response_free(resp);
    if (!json) return;

    cJSON *books = cJSON_GetObjectItem(json, "books");
    if (!books) books = json;  /* Try root as array */

    if (cJSON_IsArray(books)) {
        cJSON *book;
        cJSON_ArrayForEach(book, books) {
            cJSON *info = cJSON_GetObjectItem(book, "bookInfo");
            if (!info) info = book;

            cJSON *bid = cJSON_GetObjectItem(info, "bookId");
            if (!bid) bid = cJSON_GetObjectItem(book, "bookId");
            cJSON *title = cJSON_GetObjectItem(info, "title");
            cJSON *author = cJSON_GetObjectItem(info, "author");

            if (bid && title) {
                add_book_to_panel(recommend_container,
                                  cJSON_IsString(bid) ? bid->valuestring : "",
                                  cJSON_IsString(title) ? title->valuestring : "未知",
                                  cJSON_IsString(author) ? author->valuestring : "");
            }
        }
    }

    cJSON_Delete(json);
    printf("[DISCOVER] Loaded %d recommended books\n", discover_book_count);
}

lv_obj_t *screen_discover_create(void)
{
    discover_book_count = 0;
    category_count = 0;

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF7F4EF), 0);

    /* Header with back button and title */
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, lv_pct(100), 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_white(), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(header, 10, 0);

    lv_obj_t *back_btn = lv_button_create(header);
    lv_obj_set_size(back_btn, 60, 36);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "← 返回");
    if (chinese_font) lv_obj_set_style_text_font(back_lbl, chinese_font, 0);
    lv_obj_center(back_lbl);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "  发现");
    if (chinese_font) lv_obj_set_style_text_font(title, chinese_font, 0);

    /* Tab buttons */
    tab_btns = lv_obj_create(screen);
    lv_obj_set_size(tab_btns, lv_pct(100), 40);
    lv_obj_align(tab_btns, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(tab_btns, lv_color_white(), 0);
    lv_obj_set_flex_flow(tab_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_btns, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(tab_btns, 20, 0);

    lv_obj_t *tab1 = lv_button_create(tab_btns);
    lv_obj_set_size(tab1, 100, 32);
    lv_obj_set_style_bg_color(tab1, lv_color_hex(0x07C160), 0);
    lv_obj_add_event_cb(tab1, tab_switch_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(tab1, (void *)(intptr_t)0);
    lv_obj_t *tab1_lbl = lv_label_create(tab1);
    lv_label_set_text(tab1_lbl, "分类");
    if (chinese_font) lv_obj_set_style_text_font(tab1_lbl, chinese_font, 0);
    lv_obj_center(tab1_lbl);

    lv_obj_t *tab2 = lv_button_create(tab_btns);
    lv_obj_set_size(tab2, 100, 32);
    lv_obj_set_style_bg_color(tab2, lv_color_hex(0xCCCCCC), 0);
    lv_obj_add_event_cb(tab2, tab_switch_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(tab2, (void *)(intptr_t)1);
    lv_obj_t *tab2_lbl = lv_label_create(tab2);
    lv_label_set_text(tab2_lbl, "推荐");
    if (chinese_font) lv_obj_set_style_text_font(tab2_lbl, chinese_font, 0);
    lv_obj_center(tab2_lbl);

    /* Categories container (left: categories, right: books) */
    category_container = lv_obj_create(screen);
    lv_obj_set_size(category_container, lv_pct(95), lv_pct(78));
    lv_obj_align(category_container, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(category_container, lv_color_hex(0xF7F4EF), 0);
    lv_obj_set_style_border_width(category_container, 0, 0);
    lv_obj_set_flex_flow(category_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(category_container, 10, 0);
    lv_obj_set_style_pad_all(category_container, 0, 0);

    /* Category list (left side) */
    category_list = lv_obj_create(category_container);
    lv_obj_set_size(category_list, lv_pct(38), lv_pct(100));
    lv_obj_set_style_bg_color(category_list, lv_color_white(), 0);
    lv_obj_set_style_border_width(category_list, 1, 0);
    lv_obj_set_style_border_color(category_list, lv_color_hex(0xE7E1D7), 0);
    lv_obj_set_flex_flow(category_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(category_list, LV_DIR_VER);
    lv_obj_set_style_pad_all(category_list, 0, 0);
    lv_obj_set_style_pad_gap(category_list, 2, 0);

    /* Book panel (right side) */
    book_panel = lv_obj_create(category_container);
    lv_obj_set_size(book_panel, lv_pct(60), lv_pct(100));
    lv_obj_set_style_bg_color(book_panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(book_panel, 1, 0);
    lv_obj_set_style_border_color(book_panel, lv_color_hex(0xE7E1D7), 0);
    lv_obj_set_style_pad_all(book_panel, 0, 0);

    book_list_widget = lv_obj_create(book_panel);
    lv_obj_set_size(book_list_widget, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(book_list_widget, lv_color_white(), 0);
    lv_obj_set_style_border_width(book_list_widget, 0, 0);
    lv_obj_set_flex_flow(book_list_widget, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(book_list_widget, LV_DIR_VER);
    lv_obj_set_style_pad_all(book_list_widget, 0, 0);
    lv_obj_set_style_pad_gap(book_list_widget, 2, 0);

    /* Recommend container (hidden initially) */
    recommend_container = lv_obj_create(screen);
    lv_obj_set_size(recommend_container, lv_pct(95), lv_pct(78));
    lv_obj_align(recommend_container, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(recommend_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(recommend_container, 1, 0);
    lv_obj_set_flex_flow(recommend_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(recommend_container, LV_DIR_VER);
    lv_obj_set_style_pad_gap(recommend_container, 2, 0);
    lv_obj_add_flag(recommend_container, LV_OBJ_FLAG_HIDDEN);

    /* Load data */
    load_categories();
    load_recommend_books();

    return screen;
}
