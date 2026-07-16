/**
 * Screen Manager
 *
 * Stack-based navigation: push/pop screens with fade animations.
 */

#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "lvgl.h"

typedef enum {
    SCREEN_WELCOME,
    SCREEN_LOGIN,
    SCREEN_SHELF,
    SCREEN_DISCOVER,
    SCREEN_BOOK_DETAIL,
    SCREEN_READER
} screen_id_t;

#define MAX_SCREEN_STACK 10

/**
 * Initialize screen manager
 */
void screen_manager_init(void);

/**
 * Push a new screen onto the navigation stack (with fade animation)
 */
void screen_manager_push(screen_id_t screen_id);

/**
 * Pop the current screen and return to the previous one
 */
void screen_manager_pop(void);

/**
 * Replace the current screen (no stack change)
 */
void screen_manager_replace(screen_id_t screen_id);

/**
 * Navigate to a screen (legacy API, pushes onto stack)
 */
void screen_manager_show(screen_id_t screen_id);

/**
 * Get current screen ID
 */
screen_id_t screen_manager_get_current(void);

/**
 * Go back to previous screen (alias for pop)
 */
void screen_manager_back(void);

/* Screen creation with parameters */

/**
 * Create and push book detail screen for a specific book
 */
void screen_manager_push_book_detail(const char *book_id);

/**
 * Create and push reader screen for a specific chapter
 */
void screen_manager_push_reader(const char *book_id, int chapter_uid, const char *book_format);

#endif /* SCREEN_MANAGER_H */
