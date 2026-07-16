/**
 * Reader Screen
 */

#ifndef SCREEN_READER_H
#define SCREEN_READER_H

#include "lvgl.h"

lv_obj_t *screen_reader_create(const char *book_id, int chapter_uid, const char *book_format);

#endif /* SCREEN_READER_H */
