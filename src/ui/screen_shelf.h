/**
 * WeRead Shelf Screen
 * Displays user's book collection
 */

#ifndef SCREEN_SHELF_H
#define SCREEN_SHELF_H

#include "lvgl.h"

/**
 * Create the shelf screen
 * @return The screen object
 */
lv_obj_t *screen_shelf_create(void);

/**
 * Load and display books from the shelf
 */
void screen_shelf_load_books(void);

/**
 * Cleanup shelf screen resources
 */
void screen_shelf_cleanup(void);

/**
 * Set callback for when a book is selected
 * @param callback Function to call with book_id when book is clicked
 */
void screen_shelf_set_book_callback(void (*callback)(const char *book_id));

#endif /* SCREEN_SHELF_H */
