/**
 * @file lv_conf.h
 * Configuration file for LVGL v9.2.2 - PC Simulator
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/*====================
   COLOR SETTINGS
 *====================*/

#define LV_COLOR_DEPTH 32

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/*====================
   HAL SETTINGS
 *====================*/

#define LV_DEF_REFR_PERIOD  33
#define LV_DPI_DEF 130

#define LV_DRAW_THREAD_STACK_SIZE 32768

/*=================
 * OPERATING SYSTEM
 *=================*/

#define LV_USE_OS   LV_OS_PTHREAD

/*=========================
 *  DEMO USAGE
 *=========================*/

#define LV_USE_DEMO_WIDGETS        0
#define LV_USE_DEMO_BENCHMARK      0
#define LV_USE_DEMO_STRESS         0
#define LV_USE_DEMO_MUSIC          0

/*===================
 *  INPUT DEVICES
 *==================*/

#define LV_USE_INDEV     1
#define LV_USE_GROUP     1

/*==================
 * CORE SETTINGS
 *==================*/

#define LV_USE_OBJ_ID_BUILTIN  0
#define LV_USE_ASSERT_NULL     1
#define LV_USE_ASSERT_MALLOC   1
#define LV_USE_ASSERT_STYLE    0
#define LV_USE_ASSERT_OBJ      0

/*=========================
 *  DISPLAY CONFIGURATION
 *=========================*/

#define LV_USE_DISPLAY     1
#define LV_USE_DRAW_SW     1
#define LV_USE_DRAW_SDL    1

/*=========================
 * SDL DRIVER SETTINGS
 *=========================*/

#define LV_USE_SDL         1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE  LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT    1
#define LV_SDL_FULLSCREEN   0
#define LV_SDL_ACCELERATED  1

/*==================
 *  THEME SETTINGS
 *==================*/

#define LV_USE_THEME_DEFAULT     1
#define LV_USE_THEME_SIMPLE      1
#define LV_USE_THEME_MONO        1
#define LV_THEME_DEFAULT_INCLUDE <stdint.h>
#define LV_THEME_DEFAULT_INIT     lv_theme_default_init
#define LV_THEME_DEFAULT_COLOR_PRIMARY   lv_color_hex(0x2196f3)
#define LV_THEME_DEFAULT_COLOR_SECONDARY lv_color_hex(0x4caf50)
#define LV_THEME_DEFAULT_DARK            0
#define LV_THEME_DEFAULT_GROW            1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/*==================
 *  WIDGETS
 *==================*/

#define LV_USE_ANIMIMG    0
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BUTTON     1
#define LV_USE_BUTTONMATRIX  1
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     1
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMAGE      1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD   0
#define LV_USE_LABEL      1
#define LV_USE_LED        1
#define LV_USE_LINE       1
#define LV_USE_LIST       1
#define LV_USE_LOTTIE     0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     1
#define LV_USE_ROLLER     0
#define LV_USE_SCALE      0
#define LV_USE_SLIDER     1
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    1
#define LV_USE_SWITCH     1
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TEXTAREA   1
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*==================
 *  LAYOUTS
 *==================*/

#define LV_USE_FLEX     1
#define LV_USE_GRID     1

/*==================
 *  OTHERS
 *==================*/

#define LV_USE_SYSMON   0
#define LV_USE_SNAPSHOT 0
#define LV_USE_MONKEY   0
#define LV_USE_GRIDNAV  0
#define LV_USE_FRAGMENT 0
#define LV_USE_OBSERVER 0
#define LV_USE_PNG      0
#define LV_USE_BMP      0
#define LV_USE_TJPGD    1
#define LV_USE_LIBJPEG_TURBO 0
#define LV_USE_GIF      0
#define LV_USE_QRCODE   1
#define LV_USE_FREETYPE 1
#define LV_USE_TINY_TTF 0
#define LV_USE_RLE      0
#define LV_USE_IMGFONT  0
#define LV_USE_FILESYSTEM 1
#define LV_USE_FS_POSIX  1
#define LV_USE_FS_STDIO  0
#define LV_FS_POSIX_LETTER '/'
#define LV_FS_POSIX_PATH "/"

/*==================
 * LOG SETTINGS
 *==================*/

#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF   1
#define LV_LOG_TRACE_MEM        0
#define LV_LOG_TRACE_TIMER      0
#define LV_LOG_TRACE_INDEV      0
#define LV_LOG_TRACE_OBJ_CREATE 0
#define LV_LOG_TRACE_REFRESH    0
#define LV_LOG_TRACE_LAYOUT     0
#define LV_LOG_TRACE_ANIM       0
#define LV_LOG_TRACE_CACHE      0

/*==================
 *  FONT USAGE
 *==================*/

#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_MONTSERRAT_12_SUBPX 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK 1
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_FONT_FMT_TXT_LARGE 1

/*====================
 *  TEXT SETTINGS
 *====================*/

#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD "#"
#define LV_USE_BIDI 0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

#endif /*LV_CONF_H*/
