/**
 * WeRead API Client Implementation
 */

#include "api_client.h"
#include "../utils/crypto.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static cookie_jar_t cookie_jar = {0};
static CURL *curl = NULL;

/* Callback for capturing Set-Cookie headers (wr_* cookies) */
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    (void)userdata;
    size_t total = size * nitems;

    /* Look for Set-Cookie headers with wr_* cookies */
    const char *prefix = "Set-Cookie:";
    if (total > 11 && strncasecmp(buffer, prefix, 11) == 0) {
        const char *cookie_start = buffer + 11;
        while (*cookie_start == ' ') cookie_start++;

        /* Check if it's a wr_* cookie */
        if (strncmp(cookie_start, "wr_", 3) == 0) {
            /* Extract just the name=value part (before first ;) */
            const char *semi = strchr(cookie_start, ';');
            size_t cookie_len = semi ? (size_t)(semi - cookie_start) : total - (cookie_start - buffer);
            if (cookie_len > 0 && cookie_len < MAX_COOKIE_LEN && cookie_jar.count < MAX_COOKIES) {
                char cookie_str[MAX_COOKIE_LEN];
                memcpy(cookie_str, cookie_start, cookie_len);
                cookie_str[cookie_len] = '\0';

                /* Check if this cookie name already exists (update it) */
                char name[64] = {0};
                const char *eq = strchr(cookie_str, '=');
                if (eq) {
                    size_t name_len = eq - cookie_str;
                    if (name_len < sizeof(name)) {
                        memcpy(name, cookie_str, name_len);
                    }
                }

                /* Skip empty cookie values (e.g. "wr_vid=") — these come from
                 * renewal and would overwrite valid session cookies */
                if (eq && *(eq + 1) == '\0') {
                    return total;  /* Don't store or update empty values */
                }

                bool updated = false;
                if (name[0]) {
                    for (int i = 0; i < cookie_jar.count; i++) {
                        if (strncmp(cookie_jar.cookies[i], name, strlen(name)) == 0 &&
                            cookie_jar.cookies[i][strlen(name)] == '=') {
                            strncpy(cookie_jar.cookies[i], cookie_str, MAX_COOKIE_LEN - 1);
                            updated = true;
                            break;
                        }
                    }
                }

                if (!updated) {
                    strncpy(cookie_jar.cookies[cookie_jar.count], cookie_str, MAX_COOKIE_LEN - 1);
                    cookie_jar.count++;
                    printf("[COOKIE] Captured: %s\n", cookie_str);
                }
            }
        }
    }

    return total;
}

/* Callback for writing response data */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    api_response_t *response = (api_response_t *)userp;

    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Not enough memory\n");
        return 0;
    }

    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;

    return realsize;
}

void api_client_init(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
}

void api_client_cleanup(void)
{
    if (curl) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }
    curl_global_cleanup();
}

void api_set_cookies(const char *cookie_str)
{
    if (!cookie_str) return;

    if (cookie_jar.count < MAX_COOKIES) {
        strncpy(cookie_jar.cookies[cookie_jar.count], cookie_str, MAX_COOKIE_LEN - 1);
        cookie_jar.count++;
    }
}

const char *api_get_cookies(void)
{
    static char all_cookies[MAX_COOKIES * MAX_COOKIE_LEN];
    all_cookies[0] = '\0';

    for (int i = 0; i < cookie_jar.count; i++) {
        if (i > 0) strcat(all_cookies, "; ");
        strcat(all_cookies, cookie_jar.cookies[i]);
    }

    return all_cookies;
}

/* Cookie file path: ~/.weread-cli/cookie.json (JIX-compatible) */
static void get_cookie_path(char *buf, size_t buf_size)
{
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, buf_size, "%s/.weread-cli/cookie.json", home);
}

static void ensure_config_dir(void)
{
    char dir[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(dir, sizeof(dir), "%s/.weread-cli", home);
    mkdir(dir, 0755);
}

bool api_save_cookies(void)
{
    if (cookie_jar.count == 0) return false;

    ensure_config_dir();

    char path[512];
    get_cookie_path(path, sizeof(path));

    const char *cookies = api_get_cookies();
    if (!cookies || !cookies[0]) return false;

    FILE *f = fopen(path, "w");
    if (!f) {
        printf("[COOKIE] Failed to save cookies to %s\n", path);
        return false;
    }

    /* Write JIX-compatible JSON format */
    fprintf(f, "{\"cookie\": \"%s\"}\n", cookies);
    fclose(f);

    printf("[COOKIE] Saved cookies to %s\n", path);
    return true;
}

bool api_load_cookies(void)
{
    char path[512];
    get_cookie_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("[COOKIE] No saved cookies found\n");
        return false;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 8192) {
        fclose(f);
        return false;
    }

    char *json_str = malloc(fsize + 1);
    if (!json_str) { fclose(f); return false; }

    size_t read = fread(json_str, 1, fsize, f);
    json_str[read] = '\0';
    fclose(f);

    /* Parse JSON: {"cookie": "wr_vid=...; wr_skey=..."} */
    cJSON *json = cJSON_Parse(json_str);
    free(json_str);

    if (!json) {
        printf("[COOKIE] Failed to parse cookie file\n");
        return false;
    }

    cJSON *cookie_val = cJSON_GetObjectItem(json, "cookie");
    if (!cookie_val || !cJSON_IsString(cookie_val) || !cookie_val->valuestring) {
        cJSON_Delete(json);
        return false;
    }

    /* Parse cookie string into individual cookies */
    cookie_jar.count = 0;
    char *cookie_copy = strdup(cookie_val->valuestring);
    cJSON_Delete(json);

    char *saveptr = NULL;
    char *token = strtok_r(cookie_copy, ";", &saveptr);
    while (token && cookie_jar.count < MAX_COOKIES) {
        /* Trim leading spaces */
        while (*token == ' ') token++;
        if (*token && strncmp(token, "wr_", 3) == 0) {
            strncpy(cookie_jar.cookies[cookie_jar.count], token, MAX_COOKIE_LEN - 1);
            cookie_jar.count++;
        }
        token = strtok_r(NULL, ";", &saveptr);
    }

    free(cookie_copy);
    printf("[COOKIE] Loaded %d cookies from %s\n", cookie_jar.count, path);
    return cookie_jar.count > 0;
}

void api_clear_cookies(void)
{
    /* Clear cookie jar */
    memset(&cookie_jar, 0, sizeof(cookie_jar));

    /* Delete cookie file */
    char path[512];
    get_cookie_path(path, sizeof(path));
    remove(path);

    printf("[COOKIE] Cookies cleared\n");
}

bool api_has_saved_cookies(void)
{
    char path[512];
    get_cookie_path(path, sizeof(path));
    return access(path, F_OK) == 0;
}

api_response_t *api_get(const char *path)
{
    if (!curl || !path) return NULL;

    /* Reset curl handle to clear any leftover options from previous requests
     * (e.g. CURLOPT_POST/CURLOPT_POSTFIELDS from a prior api_post call
     *  whose local buffer is now a dangling pointer) */
    curl_easy_reset(curl);

    api_response_t *response = calloc(1, sizeof(api_response_t));
    if (!response) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s%s", API_BASE_URL, path);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    /* Add cookies if available */
    const char *cookies = api_get_cookies();
    if (cookies[0]) {
        curl_easy_setopt(curl, CURLOPT_COOKIE, cookies);
    }

    /* Set headers */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/116.0.0.0 Safari/537.36");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        api_response_free(response);
        curl_slist_free_all(headers);
        return NULL;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    response->status_code = (int)response_code;

    curl_slist_free_all(headers);
    return response;
}

api_response_t *api_post(const char *path, const char *json_body)
{
    if (!curl || !path) return NULL;

    /* Reset to clear previous request options */
    curl_easy_reset(curl);

    api_response_t *response = calloc(1, sizeof(api_response_t));
    if (!response) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s%s", API_BASE_URL, path);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    if (json_body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    }

    /* Add cookies */
    const char *cookies = api_get_cookies();
    if (cookies[0]) {
        curl_easy_setopt(curl, CURLOPT_COOKIE, cookies);
    }

    /* Set headers */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/116.0.0.0 Safari/537.36");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        api_response_free(response);
        curl_slist_free_all(headers);
        return NULL;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    response->status_code = (int)response_code;

    curl_slist_free_all(headers);
    return response;
}

api_response_t *api_post_signed(const char *path, const char **keys,
                                const char **values, int count)
{
    /* Add signature to parameters */
    char signature[65];
    if (sign_payload(keys, values, count, signature) != 0) {
        return NULL;
    }

    /* Build JSON body with signature */
    char json_body[4096] = "{";
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(json_body, ",");
        strcat(json_body, "\"");
        strcat(json_body, keys[i]);
        strcat(json_body, "\":\"");
        strcat(json_body, values[i]);
        strcat(json_body, "\"");
    }
    strcat(json_body, ",\"s\":\"");
    strcat(json_body, signature);
    strcat(json_body, "\"}");

    return api_post(path, json_body);
}

void api_response_free(api_response_t *response)
{
    if (response) {
        free(response->data);
        free(response);
    }
}

/* Authentication APIs */

api_response_t *api_auth_get_uid(void)
{
    return api_post("/web/login/getuid", "{}");
}

api_response_t *api_auth_get_info(const char *uid)
{
    char json[256];
    snprintf(json, sizeof(json), "{\"uid\":\"%s\"}", uid);
    return api_post("/web/login/getinfo", json);
}

api_response_t *api_auth_web_login(const char *skey, const char *vid, const char *code)
{
    char json[512];
    snprintf(json, sizeof(json),
             "{\"skey\":\"%s\",\"vid\":\"%s\",\"code\":\"%s\",\"fp\":\"\"}",
             skey, vid, code);
    return api_post("/web/login/weblogin", json);
}

api_response_t *api_auth_web_login_raw(const char *full_payload)
{
    if (!full_payload) return NULL;
    return api_post("/web/login/weblogin", full_payload);
}

api_response_t *api_auth_session_init(const char *vid, const char *access_token,
                                      const char *refresh_token)
{
    char json[1024];
    snprintf(json, sizeof(json),
             "{\"vid\":\"%s\",\"pf\":0,\"skey\":\"%s\",\"rt\":\"%s\"}",
             vid, access_token, refresh_token);
    return api_post("/web/login/session/init", json);
}

api_response_t *api_auth_renewal(const char *rq)
{
    char json[1024];
    snprintf(json, sizeof(json), "{\"rq\":\"%s\"}", rq);
    return api_post("/web/login/renewal", json);
}

/* Shelf and Book APIs */

api_response_t *api_get_shelf(void)
{
    return api_get("/web/shelf/sync");
}

api_response_t *api_get_book_info(const char *book_id)
{
    char path[256];
    snprintf(path, sizeof(path), "/web/book/info?bookId=%s", book_id);
    return api_get(path);
}

api_response_t *api_get_chapters(const char *book_id)
{
    char json[256];
    snprintf(json, sizeof(json), "{\"bookIds\":[\"%s\"]}", book_id);
    return api_post("/web/book/chapterInfos", json);
}

api_response_t *api_get_chapter_content(const char *book_id, int chapter_uid,
                                        const char *format)
{
    /* Calculate hashes */
    char b_hash[64], c_hash[64];
    char chapter_uid_str[32];
    snprintf(chapter_uid_str, sizeof(chapter_uid_str), "%d", chapter_uid);

    calc_hash(book_id, b_hash);
    calc_hash(chapter_uid_str, c_hash);

    /* Build parameters */
    const char *keys[] = {"b", "c", "r", "st", "ct"};
    char ct[32];
    snprintf(ct, sizeof(ct), "%ld", time(NULL));

    const char *values[] = {b_hash, c_hash, "0", "0", ct};

    /* Determine endpoint based on format */
    char path[64];
    if (strcmp(format, "epub") == 0 || strcmp(format, "pdf") == 0) {
        snprintf(path, sizeof(path), "/web/book/chapter/e_0");
    } else {
        snprintf(path, sizeof(path), "/web/book/chapter/t_0");
    }

    return api_post_signed(path, keys, values, 5);
}

api_response_t *api_read_init(const char *book_id, int chapter_uid,
                              int chapter_offset, int percent)
{
    char b_hash[64], c_hash[64];
    char chapter_uid_str[32], offset_str[32], percent_str[32], ct[32];

    snprintf(chapter_uid_str, sizeof(chapter_uid_str), "%d", chapter_uid);
    snprintf(offset_str, sizeof(offset_str), "%d", chapter_offset);
    snprintf(percent_str, sizeof(percent_str), "%d", percent);
    snprintf(ct, sizeof(ct), "%ld", time(NULL));

    calc_hash(book_id, b_hash);
    calc_hash(chapter_uid_str, c_hash);

    const char *keys[] = {"b", "c", "ci", "co", "pr", "ct"};
    const char *values[] = {b_hash, c_hash, chapter_uid_str, offset_str, percent_str, ct};

    return api_post_signed("/web/book/read", keys, values, 6);
}

api_response_t *api_read_update(const char *book_id, int chapter_uid,
                                int chapter_offset, int percent,
                                const char *reader_token)
{
    /* Similar to read_init but with reader_token and additional fields */
    char b_hash[64], c_hash[64];
    char chapter_uid_str[32], offset_str[32], percent_str[32], ct[32];

    snprintf(chapter_uid_str, sizeof(chapter_uid_str), "%d", chapter_uid);
    snprintf(offset_str, sizeof(offset_str), "%d", chapter_offset);
    snprintf(percent_str, sizeof(percent_str), "%d", percent);
    snprintf(ct, sizeof(ct), "%ld", time(NULL));

    calc_hash(book_id, b_hash);
    calc_hash(chapter_uid_str, c_hash);

    const char *keys[] = {"b", "c", "ci", "co", "pr", "ct", "rt"};
    const char *values[] = {b_hash, c_hash, chapter_uid_str, offset_str,
                           percent_str, ct, reader_token};

    return api_post_signed("/web/book/read", keys, values, 7);
}

/* Progress API */

api_response_t *api_get_progress(const char *book_id)
{
    char path[256];
    snprintf(path, sizeof(path), "/web/book/getProgress?bookId=%s", book_id);
    return api_get(path);
}

/* Chapter piece API (low-level, single piece) */

api_response_t *api_get_chapter_piece(const char *book_id, int chapter_uid,
                                      const char *suffix, int st)
{
    char b_hash[64], c_hash[64];
    char chapter_uid_str[32], st_str[8], ct[32], r_str[32];

    snprintf(chapter_uid_str, sizeof(chapter_uid_str), "%d", chapter_uid);
    snprintf(st_str, sizeof(st_str), "%d", st);
    snprintf(ct, sizeof(ct), "%ld", time(NULL));
    snprintf(r_str, sizeof(r_str), "%d", (rand() % 10000) * (rand() % 10000));

    calc_hash(book_id, b_hash);
    calc_hash(chapter_uid_str, c_hash);

    const char *keys[] = {"b", "c", "r", "st", "ct", "ps", "pc"};
    const char *values[] = {b_hash, c_hash, r_str, st_str, ct,
                            "a2b325707a19e580g0186a2",
                            "430321207a19e581g013ab0"};

    char path[64];
    snprintf(path, sizeof(path), "/web/book/chapter/%s", suffix);

    return api_post_signed(path, keys, values, 7);
}

/* Full chapter content fetch with decryption */

int api_fetch_chapter_content(const char *book_id, int chapter_uid,
                              const char *book_format,
                              char **html_out, char **style_out)
{
    if (!book_id || !html_out) return -1;

    *html_out = NULL;
    if (style_out) *style_out = NULL;

    int is_txt = (book_format && strcmp(book_format, "txt") == 0);
    int piece_count = is_txt ? 2 : 4;
    const char *prefix = is_txt ? "t" : "e";

    /* St values for each piece: epub=[0,0,1,0], txt=[0,1] */
    int st_values[] = {0, 0, 1, 0};
    if (is_txt) {
        st_values[0] = 0;
        st_values[1] = 1;
    }

    /* Fetch all pieces */
    char *pieces[4] = {NULL};
    for (int i = 0; i < piece_count; i++) {
        char suffix[8];
        snprintf(suffix, sizeof(suffix), "%s_%d", prefix, i);

        api_response_t *resp = api_get_chapter_piece(book_id, chapter_uid, suffix, st_values[i]);
        if (!resp || !resp->data || resp->size == 0) {
            printf("[CONTENT] Failed to fetch piece %s\n", suffix);
            api_response_free(resp);
            /* Cleanup previously fetched pieces */
            for (int j = 0; j < i; j++) free(pieces[j]);
            return -1;
        }

        /* Verify integrity with chk() and strip MD5 header */
        char *verified;
        if (chk(resp->data, NULL)) {
            /* MD5 header verified — use body (data after 32-char header) */
            const char *body = chk_body(resp->data);
            verified = strdup(body ? body : "");
            printf("[CONTENT] Piece %s: chk OK, body len=%zu\n", suffix, strlen(verified));
        } else {
            /* chk failed or data too short — use raw data */
            verified = strdup(resp->data);
            printf("[CONTENT] Piece %s: chk skip, raw len=%zu\n", suffix, strlen(verified));
        }
        pieces[i] = verified;
        api_response_free(resp);
    }

    /* Assemble and decrypt */
    if (is_txt) {
        /* TXT: html = decrypt_wr(p0 + p1) */
        size_t total = strlen(pieces[0]) + strlen(pieces[1]) + 1;
        char *combined = malloc(total);
        snprintf(combined, total, "%s%s", pieces[0], pieces[1]);

        char *decrypted = malloc(total * 2);
        int dec_len = decrypt_wr(combined, decrypted, total * 2);
        free(combined);

        if (dec_len > 0) {
            *html_out = decrypted;
        } else {
            *html_out = decrypted;
        }
    } else {
        /* EPUB: html = decrypt_wr(p0 + p1 + p3), style = decrypt_wr(p2) */
        size_t html_total = strlen(pieces[0]) + strlen(pieces[1]) + strlen(pieces[3]) + 1;
        char *html_combined = malloc(html_total);
        snprintf(html_combined, html_total, "%s%s%s", pieces[0], pieces[1], pieces[3]);

        char *html_decrypted = malloc(html_total * 2);
        decrypt_wr(html_combined, html_decrypted, html_total * 2);
        *html_out = html_decrypted;
        free(html_combined);

        if (style_out && pieces[2]) {
            size_t style_total = strlen(pieces[2]) + 1;
            char *style_decrypted = malloc(style_total * 2);
            decrypt_wr(pieces[2], style_decrypted, style_total * 2);
            *style_out = style_decrypted;
        }
    }

    /* Cleanup pieces */
    for (int i = 0; i < piece_count; i++) {
        free(pieces[i]);
    }

    printf("[CONTENT] Chapter %d content fetched (%d pieces)\n", chapter_uid, piece_count);
    return 0;
}

/* Underlines / Highlights API */

api_response_t *api_get_underlines(const char *book_id, int chapter_uid)
{
    char path[256];
    snprintf(path, sizeof(path), "/web/book/underlines?bookId=%s&chapterUid=%d",
             book_id, chapter_uid);
    return api_get(path);
}

/* Discover / Category APIs */

api_response_t *api_get_categories(void)
{
    return api_get("/web/categories?synckey=0");
}

api_response_t *api_get_category_books(const char *category_id)
{
    char path[256];
    snprintf(path, sizeof(path), "/web/bookListInCategory/%s?maxIndex=0", category_id);
    return api_get(path);
}

api_response_t *api_get_recommend_books(void)
{
    return api_get("/web/recommend_books");
}
