/**
 * WeRead Login Screen
 *
 * Displays QR code for WeChat authentication
 */

#ifndef SCREEN_LOGIN_H
#define SCREEN_LOGIN_H

#include "lvgl.h"
#include <stdbool.h>

/**
 * Create login screen
 */
lv_obj_t *screen_login_create(void);

/**
 * Start login flow
 */
void screen_login_start(void);

/**
 * Update login screen (call periodically to poll auth status)
 */
void screen_login_update(void);

/**
 * Set callback for successful login
 */
void screen_login_set_success_callback(void (*callback)(void));

#endif /* SCREEN_LOGIN_H */
