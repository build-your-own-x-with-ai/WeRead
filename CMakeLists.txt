cmake_minimum_required(VERSION 3.16)
project(WeReadLVGL C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(OPENSSL REQUIRED openssl)

# LVGL configuration
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)

# LVGL library
add_subdirectory(lib/lvgl)
target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
target_compile_options(lvgl PUBLIC ${SDL2_CFLAGS_OTHER})

# cJSON library
add_library(cjson STATIC
    lib/cJSON/cJSON.c
)
target_include_directories(cjson PUBLIC lib/cJSON)

# qrcodegen library
add_library(qrcodegen STATIC
    lib/qrcodegen/qrcodegen.c
)
target_include_directories(qrcodegen PUBLIC lib/qrcodegen)

# Application sources
file(GLOB_RECURSE APP_SOURCES
    src/*.c
    src/api/*.c
    src/ui/*.c
    src/ui/screens/*.c
    src/ui/widgets/*.c
    src/utils/*.c
)

# Main executable
add_executable(weread_lvgl ${APP_SOURCES})

target_include_directories(weread_lvgl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

target_link_directories(weread_lvgl PRIVATE
    ${OPENSSL_LIBRARY_DIRS}
)

target_link_libraries(weread_lvgl PRIVATE
    lvgl
    lvgl::examples
    lvgl::demos
    cjson
    qrcodegen
    ${SDL2_LIBRARIES}
    ${CURL_LIBRARIES}
    ssl
    crypto
    m
)

target_compile_options(weread_lvgl PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    ${SDL2_CFLAGS_OTHER}
)

# Installation
install(TARGETS weread_lvgl DESTINATION bin)
