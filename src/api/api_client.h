/**
 * WeRead API Client
 *
 * HTTP client for WeRead API with authentication and request signing
 */

#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define API_BASE_URL "https://weread.qq.com"
#define MAX_COOKIES 16
#define MAX_COOKIE_LEN 512

/* Cookie storage */
typedef struct {
    char cookies[MAX_COOKIES][MAX_COOKIE_LEN];
    int count;
} cookie_jar_t;

/* API response */
typedef struct {
    char *data;
    size_t size;
    int status_code;
} api_response_t;

/**
 * Initialize API client
 */
void api_client_init(void);

/**
 * Cleanup API client resources
 */
void api_client_cleanup(void);

/**
 * Set cookies from string
 */
void api_set_cookies(const char *cookie_str);

/**
 * Get current cookies
 */
const char *api_get_cookies(void);

/**
 * Save current cookies to ~/.weread-cli/cookie.json (JIX-compatible format)
 */
bool api_save_cookies(void);

/**
 * Load cookies from ~/.weread-cli/cookie.json and restore to cookie jar.
 * Returns true if cookies were loaded successfully.
 */
bool api_load_cookies(void);

/**
 * Clear cookie jar and delete saved cookie file.
 */
void api_clear_cookies(void);

/**
 * Check if saved cookie file exists.
 */
bool api_has_saved_cookies(void);

/**
 * Make GET request
 */
api_response_t *api_get(const char *path);

/**
 * Make POST request with JSON body
 */
api_response_t *api_post(const char *path, const char *json_body);

/**
 * Make signed POST request (for reading/chapter endpoints)
 */
api_response_t *api_post_signed(const char *path, const char **keys,
                                const char **values, int count);

/**
 * Free API response
 */
void api_response_free(api_response_t *response);

/**
 * Authentication: Get UID for QR login
 * Returns: {uid: string, qrImageUrl: string}
 */
api_response_t *api_auth_get_uid(void);

/**
 * Authentication: Get login info (polling)
 * Returns: {status: int, skey: string, vid: string, code: string}
 */
api_response_t *api_auth_get_info(const char *uid);

/**
 * Authentication: Web login (polling)
 * Returns: {accessToken: string, refreshToken: string, vid: string}
 */
api_response_t *api_auth_web_login(const char *skey, const char *vid, const char *code);

/**
 * Authentication: Web login with full getinfo JSON payload (JIX-compatible)
 * Takes the complete getinfo response JSON, strips unnecessary fields, adds fp=""
 */
api_response_t *api_auth_web_login_raw(const char *full_payload);

/**
 * Authentication: Init session
 */
api_response_t *api_auth_session_init(const char *vid, const char *access_token,
                                      const char *refresh_token);

/**
 * Authentication: Renewal
 */
api_response_t *api_auth_renewal(const char *rq);

/**
 * Get bookshelf
 */
api_response_t *api_get_shelf(void);

/**
 * Get book info
 */
api_response_t *api_get_book_info(const char *book_id);

/**
 * Get chapter list
 */
api_response_t *api_get_chapters(const char *book_id);

/**
 * Get reading progress for a book
 * Returns: {book: {progress: int, chapterUid: int, ...}}
 */
api_response_t *api_get_progress(const char *book_id);

/**
 * Get chapter content (encrypted, single piece)
 */
api_response_t *api_get_chapter_content(const char *book_id, int chapter_uid,
                                        const char *format);

/**
 * Get a single encrypted chapter piece (low-level)
 * @param book_id Book ID
 * @param chapter_uid Chapter UID
 * @param suffix Piece suffix: "e_0".."e_3" for epub, "t_0".."t_1" for txt
 * @param st st parameter (0 or 1)
 * Returns raw encrypted piece text
 */
api_response_t *api_get_chapter_piece(const char *book_id, int chapter_uid,
                                      const char *suffix, int st);

/**
 * Fetch and decrypt full chapter content (4-piece for epub, 2-piece for txt)
 * Returns decrypted HTML string (caller must free), or NULL on failure.
 * @param book_id Book ID
 * @param chapter_uid Chapter UID
 * @param book_format "epub"/"pdf" (4 pieces) or "txt" (2 pieces)
 * @param html_out Output pointer for decrypted HTML (caller must free)
 * @param style_out Output pointer for decrypted CSS (caller must free, may be NULL)
 * @return 0 on success, -1 on failure
 */
int api_fetch_chapter_content(const char *book_id, int chapter_uid,
                              const char *book_format,
                              char **html_out, char **style_out);

/**
 * Get highlights/underlines for a chapter
 */
api_response_t *api_get_underlines(const char *book_id, int chapter_uid);

/**
 * Get categories (no auth required)
 */
api_response_t *api_get_categories(void);

/**
 * Get books in a category (no auth required)
 */
api_response_t *api_get_category_books(const char *category_id);

/**
 * Get recommended books
 */
api_response_t *api_get_recommend_books(void);

/**
 * Initialize reading session
 */
api_response_t *api_read_init(const char *book_id, int chapter_uid,
                              int chapter_offset, int percent);

/**
 * Update reading progress
 */
api_response_t *api_read_update(const char *book_id, int chapter_uid,
                                int chapter_offset, int percent,
                                const char *reader_token);

#endif /* API_CLIENT_H */
