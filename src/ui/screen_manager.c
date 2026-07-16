/**
 * Screen Manager Implementation
 *
 * Stack-based navigation with fade animations.
 */

#include "screen_manager.h"
#include "screen_login.h"
#include "screen_shelf.h"
#include "screen_discover.h"
#include "screen_book_detail.h"
#include "screen_reader.h"
#include "../api/api_client.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Screen stack */
static screen_id_t screen_stack[MAX_SCREEN_STACK];
static int stack_size = 0;

/* Current screen object (for cleanup) */
static lv_obj_t *current_screen_obj = NULL;

/* Parameters passed to screens */
static char param_book_id[64] = {0};
static int param_chapter_uid = 0;
static char param_book_format[16] = {0};

/* Forward declarations */
extern lv_font_t *chinese_font;
static lv_obj_t *create_welcome_screen(void);
static void on_login_success(void);

void screen_manager_init(void)
{
    stack_size = 0;

    /* Check for saved cookies (auto-login) */
    if (api_has_saved_cookies() && api_load_cookies()) {
        printf("[SCREEN] Found saved cookies, auto-login to shelf\n");
        screen_manager_push(SCREEN_SHELF);
    } else {
        screen_manager_push(SCREEN_WELCOME);
    }
}

/* Create screen based on ID */
static lv_obj_t *create_screen(screen_id_t id)
{
    lv_obj_t *screen = NULL;

    switch (id) {
        case SCREEN_WELCOME:
            screen = create_welcome_screen();
            break;
        case SCREEN_LOGIN:
            screen = screen_login_create();
            screen_login_set_success_callback(on_login_success);
            screen_login_start();
            break;
        case SCREEN_SHELF:
            screen = screen_shelf_create();
            screen_shelf_load_books();
            break;
        case SCREEN_DISCOVER:
            screen = screen_discover_create();
            break;
        case SCREEN_BOOK_DETAIL:
            screen = screen_book_detail_create(param_book_id);
            break;
        case SCREEN_READER:
            screen = screen_reader_create(param_book_id, param_chapter_uid, param_book_format);
            break;
    }

    return screen;
}

void screen_manager_push(screen_id_t screen_id)
{
    lv_obj_t *screen = create_screen(screen_id);
    if (!screen) {
        printf("[SCREEN] Failed to create screen %d\n", screen_id);
        return;
    }

    /* Push current screen ID to stack */
    if (stack_size < MAX_SCREEN_STACK) {
        screen_stack[stack_size++] = screen_id;
    }

    current_screen_obj = screen;
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
}

void screen_manager_pop(void)
{
    if (stack_size <= 1) {
        printf("[SCREEN] Cannot pop, stack has only one screen\n");
        return;
    }

    /* Pop current screen */
    stack_size--;
    screen_id_t prev_id = screen_stack[stack_size - 1];

    /* Create previous screen */
    lv_obj_t *screen = create_screen(prev_id);
    if (screen) {
        current_screen_obj = screen;
        lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
    }
}

void screen_manager_replace(screen_id_t screen_id)
{
    lv_obj_t *screen = create_screen(screen_id);
    if (!screen) return;

    /* Replace top of stack */
    if (stack_size > 0) {
        screen_stack[stack_size - 1] = screen_id;
    }

    current_screen_obj = screen;
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
}

void screen_manager_show(screen_id_t screen_id)
{
    screen_manager_push(screen_id);
}

screen_id_t screen_manager_get_current(void)
{
    if (stack_size > 0) {
        return screen_stack[stack_size - 1];
    }
    return SCREEN_WELCOME;
}

void screen_manager_back(void)
{
    screen_manager_pop();
}

void screen_manager_push_book_detail(const char *book_id)
{
    strncpy(param_book_id, book_id, sizeof(param_book_id) - 1);
    screen_manager_push(SCREEN_BOOK_DETAIL);
}

void screen_manager_push_reader(const char *book_id, int chapter_uid, const char *book_format)
{
    strncpy(param_book_id, book_id, sizeof(param_book_id) - 1);
    param_chapter_uid = chapter_uid;
    if (book_format) {
        strncpy(param_book_format, book_format, sizeof(param_book_format) - 1);
    } else {
        strncpy(param_book_format, "epub", sizeof(param_book_format) - 1);
    }
    screen_manager_push(SCREEN_READER);
}

/* Callback when login succeeds */
static void on_login_success(void)
{
    screen_manager_replace(SCREEN_SHELF);
}

/* Welcome screen implementation */
static void welcome_btn_cb(lv_event_t *e)
{
    (void)e;
    screen_manager_push(SCREEN_LOGIN);
}

static lv_obj_t *create_welcome_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF7F4EF), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "欢迎使用微信读书");
    if (chinese_font) {
        lv_obj_set_style_text_font(title, chinese_font, 0);
    } else {
        lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    }
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -50);

    /* Login button */
    lv_obj_t *btn = lv_button_create(screen);
    lv_obj_set_size(btn, 200, 60);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(btn, welcome_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x07C160), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "登录");
    if (chinese_font) {
        lv_obj_set_style_text_font(btn_label, chinese_font, 0);
    }
    lv_obj_center(btn_label);

    return screen;
}
