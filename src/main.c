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

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static lv_display_t *display;
static lv_indev_t *indev_mouse;
static lv_indev_t *indev_keyboard;

static void create_test_ui(void)
{
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello LVGL!\n微信读书");
    lv_obj_center(label);

    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Initialize LVGL */
    lv_init();

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

    return 0;
}
