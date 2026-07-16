/**
 * WeRead LVGL - Main entry point
 *
 * Initializes SDL2 display driver and LVGL, then runs the main event loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl.h"
#include "lv_examples.h"
#include "lv_demos.h"
#include "ui/screen_manager.h"
#include "api/api_client.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static lv_display_t *display;
static lv_indev_t *indev_mouse;
lv_indev_t *indev_keyboard = NULL;  /* Non-static for reader keyboard input */
lv_font_t *chinese_font = NULL;  /* Body text font (18px) */
lv_font_t *chinese_font_large = NULL;  /* Header/title font (26px) */

static void create_test_ui(void)
{
    /* Initialize API client (must be called before any API requests) */
    api_client_init();

    /* Initialize screen manager - it will show the welcome screen */
    screen_manager_init();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Initialize LVGL */
    lv_init();

    /* Load Chinese fonts using FreeType - two sizes for different contexts */
    chinese_font = lv_freetype_font_create("/System/Library/Fonts/STHeiti Light.ttc",
                                           LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                           18,
                                           LV_FREETYPE_FONT_STYLE_NORMAL);
    chinese_font_large = lv_freetype_font_create("/System/Library/Fonts/STHeiti Light.ttc",
                                                  LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                                  26,
                                                  LV_FREETYPE_FONT_STYLE_NORMAL);
    if (!chinese_font || !chinese_font_large) {
        fprintf(stderr, "Warning: Failed to load Chinese font, will use default font\n");
    } else {
        printf("Chinese fonts loaded successfully (18px + 26px)\n");
    }

    /* Create SDL2 display */
    display = lv_sdl_window_create(WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!display) {
        fprintf(stderr, "Failed to create SDL2 window\n");
        return 1;
    }

    /* Create SDL2 mouse input device */
    indev_mouse = lv_sdl_mouse_create();
    if (!indev_mouse) {
        fprintf(stderr, "Failed to create mouse input device\n");
        return 1;
    }

    /* Create SDL2 keyboard input device */
    indev_keyboard = lv_sdl_keyboard_create();
    if (!indev_keyboard) {
        fprintf(stderr, "Failed to create keyboard input device\n");
        return 1;
    }

    /* Create test UI */
    create_test_ui();

    printf("WeRead LVGL started - Window: %dx%d\n", WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("Press Ctrl+C to exit\n");

    /* Main event loop */
    while (1) {
        uint32_t time_till_next = lv_timer_handler();
        usleep(time_till_next * 1000);
    }

    /* Cleanup */
    api_client_cleanup();
    if (chinese_font) {
        lv_freetype_font_delete(chinese_font);
    }
    if (chinese_font_large) {
        lv_freetype_font_delete(chinese_font_large);
    }

    return 0;
}
