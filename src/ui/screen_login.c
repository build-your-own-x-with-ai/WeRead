/**
 * WeRead Login Screen Implementation
 *
 * Uses a background pthread for HTTP long-polling to avoid blocking
 * the LVGL main thread (getinfo is a ~65s server-side long-poll).
 */

#include "screen_login.h"
#include "../auth/auth_controller.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

static lv_obj_t *screen = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *qr_code = NULL;
static lv_obj_t *instruction_label = NULL;
static lv_timer_t *poll_timer = NULL;
static void (*success_callback)(void) = NULL;
static char last_message[256] = {0};  /* Track last displayed message to avoid flicker */

/* QR code display size */
#define QR_SIZE 300

/* Background thread state */
typedef struct {
    volatile bool running;      /* Set false to stop thread */
    volatile bool done;         /* Thread sets true when finished */
    volatile bool success;      /* Login succeeded */
    char message[256];          /* Status message for UI */
    auth_state_t final_state;   /* Final auth state */
} poll_thread_ctx_t;

static poll_thread_ctx_t *g_poll_ctx = NULL;
static pthread_t g_poll_thread = 0;

/* Background thread: runs the full auth polling loop */
static void *auth_poll_thread(void *arg)
{
    poll_thread_ctx_t *ctx = (poll_thread_ctx_t *)arg;

    printf("[LOGIN-THREAD] Started polling...\n");

    while (ctx->running) {
        auth_state_t state = auth_get_state();

        switch (state) {
            case AUTH_STATE_WAIT_SCAN: {
                /* Long-poll getinfo (may block ~65s) */
                if (auth_poll_scan()) {
                    snprintf(ctx->message, sizeof(ctx->message),
                             "登录确认成功，正在初始化会话...");
                    printf("[LOGIN-THREAD] Scan+confirm detected, going to session_init!\n");
                }
                break;
            }

            case AUTH_STATE_WAIT_CONFIRM: {
                /* Poll weblogin for phone confirmation */
                if (auth_poll_confirm()) {
                    snprintf(ctx->message, sizeof(ctx->message),
                             "确认成功，正在初始化...");
                    printf("[LOGIN-THREAD] Phone confirmed!\n");
                }
                break;
            }

            case AUTH_STATE_SESSION_INIT: {
                /* Complete session */
                if (auth_complete()) {
                    snprintf(ctx->message, sizeof(ctx->message), "登录成功！");
                    ctx->success = true;
                    ctx->final_state = AUTH_STATE_SUCCESS;
                    ctx->done = true;
                    printf("[LOGIN-THREAD] Login successful!\n");
                    return NULL;
                }
                break;
            }

            case AUTH_STATE_SUCCESS:
                snprintf(ctx->message, sizeof(ctx->message), "登录成功！");
                ctx->success = true;
                ctx->done = true;
                return NULL;

            case AUTH_STATE_ERROR: {
                const auth_data_t *data = auth_get_data();
                snprintf(ctx->message, sizeof(ctx->message),
                         "登录失败: %s", data->error_msg);
                ctx->success = false;
                ctx->final_state = AUTH_STATE_ERROR;
                ctx->done = true;
                printf("[LOGIN-THREAD] Login error: %s\n", data->error_msg);
                return NULL;
            }

            default:
                break;
        }
    }

    ctx->done = true;
    return NULL;
}

/* LVGL timer callback: checks background thread status (non-blocking) */
static void poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!g_poll_ctx) return;

    /* Update status message from thread (only if changed to avoid flicker) */
    if (g_poll_ctx->message[0] &&
        strcmp(last_message, g_poll_ctx->message) != 0) {
        lv_label_set_text(status_label, g_poll_ctx->message);
        strncpy(last_message, g_poll_ctx->message, sizeof(last_message) - 1);
        last_message[sizeof(last_message) - 1] = '\0';
    }

    /* Check if thread is done */
    if (g_poll_ctx->done) {
        if (g_poll_ctx->success) {
            lv_label_set_text(status_label, "登录成功！");
            if (poll_timer) {
                lv_timer_del(poll_timer);
                poll_timer = NULL;
            }
            if (success_callback) {
                lv_timer_t *t = lv_timer_create(
                    (lv_timer_cb_t)success_callback, 1000, NULL);
                lv_timer_set_repeat_count(t, 1);
            }
        } else {
            /* Error - show message, stop timer */
            lv_label_set_text(status_label, g_poll_ctx->message);
            if (poll_timer) {
                lv_timer_del(poll_timer);
                poll_timer = NULL;
            }
        }

        /* Cleanup thread */
        pthread_join(g_poll_thread, NULL);
        g_poll_thread = 0;
        free(g_poll_ctx);
        g_poll_ctx = NULL;
    }
}

lv_obj_t *screen_login_create(void)
{
    /* Reset message tracking */
    last_message[0] = '\0';

    /* Create screen */
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF7F4EF), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "微信读书登录");
    extern lv_font_t *chinese_font;
    if (chinese_font) {
        lv_obj_set_style_text_font(title, chinese_font, 0);
    }
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* Status label */
    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "初始化中...");
    if (chinese_font) {
        lv_obj_set_style_text_font(status_label, chinese_font, 0);
    }
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 80);

    /* QR code widget */
    qr_code = lv_qrcode_create(screen);
    lv_qrcode_set_size(qr_code, QR_SIZE);
    lv_qrcode_set_dark_color(qr_code, lv_color_black());
    lv_qrcode_set_light_color(qr_code, lv_color_white());
    lv_obj_align(qr_code, LV_ALIGN_CENTER, 0, 0);

    /* Add border to QR code */
    lv_obj_set_style_border_width(qr_code, 2, 0);
    lv_obj_set_style_border_color(qr_code, lv_palette_main(LV_PALETTE_GREY), 0);

    /* Instruction label */
    instruction_label = lv_label_create(screen);
    lv_label_set_text(instruction_label, "请使用微信扫描二维码");
    if (chinese_font) {
        lv_obj_set_style_text_font(instruction_label, chinese_font, 0);
    }
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_MID, 0, -40);

    return screen;
}

void screen_login_start(void)
{
    /* Initialize auth */
    auth_init();

    /* Start auth flow (getuid - quick request, OK on main thread) */
    if (auth_start()) {
        const auth_data_t *data = auth_get_data();

        /* Display QR code */
        lv_result_t res = lv_qrcode_update(qr_code, data->qr_url, strlen(data->qr_url));
        if (res != LV_RESULT_OK) {
            printf("[LOGIN] Failed to update QR code\n");
            lv_label_set_text(status_label, "二维码生成失败");
            return;
        }

        lv_label_set_text(status_label, "请扫描二维码");

        /* Start background polling thread */
        g_poll_ctx = calloc(1, sizeof(poll_thread_ctx_t));
        g_poll_ctx->running = true;
        g_poll_ctx->done = false;

        pthread_create(&g_poll_thread, NULL, auth_poll_thread, g_poll_ctx);
        /* NOTE: Do NOT detach — we join in poll_timer_cb when thread finishes */

        /* Start LVGL timer to check thread status (every 500ms, non-blocking) */
        if (poll_timer) {
            lv_timer_del(poll_timer);
        }
        poll_timer = lv_timer_create(poll_timer_cb, 500, NULL);

        printf("[LOGIN] Background poll thread started\n");
    } else {
        lv_label_set_text(status_label, "初始化失败，请重试");
    }
}

void screen_login_update(void)
{
    /* Polling handled by timer + background thread */
}

void screen_login_set_success_callback(void (*callback)(void))
{
    success_callback = callback;
}

void screen_login_cleanup(void)
{
    /* Stop background thread */
    if (g_poll_ctx) {
        g_poll_ctx->running = false;
    }

    if (poll_timer) {
        lv_timer_del(poll_timer);
        poll_timer = NULL;
    }

    if (screen) {
        lv_obj_del(screen);
        screen = NULL;
    }

    qr_code = NULL;
    status_label = NULL;
    instruction_label = NULL;
}
