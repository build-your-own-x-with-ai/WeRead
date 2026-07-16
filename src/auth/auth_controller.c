/**
 * WeRead Authentication Controller Implementation
 *
 * 6-step QR code login flow matching JIX Python reference:
 * 1. getuid -> uid
 * 2. getinfo (poll) -> skey, vid, code (full JSON saved)
 * 3. weblogin (poll) with FULL getinfo JSON -> accessToken, refreshToken
 * 4. session_init -> cookies
 * 5. renewal -> refresh cookies
 * 6. SUCCESS
 */

#include "auth_controller.h"
#include "../api/api_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

static auth_data_t auth_data = {0};

/* Helper: extract cJSON value as string (handles string, number, bool) */
static bool cjson_get_string(cJSON *item, char *buf, size_t buf_size)
{
    if (!item) return false;
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        strncpy(buf, item->valuestring, buf_size - 1);
        buf[buf_size - 1] = '\0';
        return true;
    }
    if (cJSON_IsNumber(item)) {
        snprintf(buf, buf_size, "%lld", (long long)item->valuedouble);
        return true;
    }
    if (cJSON_IsBool(item)) {
        snprintf(buf, buf_size, "%s", cJSON_IsTrue(item) ? "true" : "false");
        return true;
    }
    return false;
}

void auth_init(void)
{
    if (auth_data.getinfo_response) {
        free(auth_data.getinfo_response);
    }
    memset(&auth_data, 0, sizeof(auth_data));
    auth_data.state = AUTH_STATE_IDLE;
}

bool auth_start(void)
{
    printf("[AUTH] Starting authentication flow...\n");
    auth_data.state = AUTH_STATE_GET_UID;

    /* Step 1: Get UID */
    api_response_t *response = api_auth_get_uid();
    if (!response || response->status_code != 200) {
        snprintf(auth_data.error_msg, sizeof(auth_data.error_msg),
                "Failed to get UID");
        auth_data.state = AUTH_STATE_ERROR;
        api_response_free(response);
        return false;
    }

    /* Parse JSON response */
    cJSON *json = cJSON_Parse(response->data);
    if (!json) {
        snprintf(auth_data.error_msg, sizeof(auth_data.error_msg),
                "Failed to parse UID response");
        auth_data.state = AUTH_STATE_ERROR;
        api_response_free(response);
        return false;
    }

    cJSON *uid = cJSON_GetObjectItem(json, "uid");
    if (uid && cJSON_IsString(uid)) {
        strncpy(auth_data.uid, uid->valuestring, sizeof(auth_data.uid) - 1);

        snprintf(auth_data.qr_url, sizeof(auth_data.qr_url),
                "https://weread.qq.com/web/confirm?pf=2&uid=%s", auth_data.uid);

        printf("[AUTH] Got UID: %s\n", auth_data.uid);
        printf("[AUTH] QR URL: %s\n", auth_data.qr_url);

        auth_data.state = AUTH_STATE_WAIT_SCAN;
        cJSON_Delete(json);
        api_response_free(response);
        return true;
    }

    snprintf(auth_data.error_msg, sizeof(auth_data.error_msg),
            "UID not found in response");
    auth_data.state = AUTH_STATE_ERROR;
    cJSON_Delete(json);
    api_response_free(response);
    return false;
}

/**
 * Build weblogin payload from getinfo response.
 * JIX: copies full getinfo dict, removes redirect_uri/expireMode/pf, adds fp="".
 */
static char *build_weblogin_payload(const char *getinfo_json)
{
    cJSON *info = cJSON_Parse(getinfo_json);
    if (!info) return NULL;

    /* Remove keys not needed for weblogin */
    cJSON_DeleteItemFromObject(info, "redirect_uri");
    cJSON_DeleteItemFromObject(info, "expireMode");
    cJSON_DeleteItemFromObject(info, "pf");
    cJSON_DeleteItemFromObject(info, "status");
    cJSON_DeleteItemFromObject(info, "errCode");
    cJSON_DeleteItemFromObject(info, "errMsg");

    /* Add fp="" */
    cJSON_AddStringToObject(info, "fp", "");

    char *payload = cJSON_PrintUnformatted(info);
    cJSON_Delete(info);
    return payload;
}

bool auth_poll_scan(void)
{
    if (auth_data.state != AUTH_STATE_WAIT_SCAN) {
        return false;
    }

    /* Step 2: Poll getinfo for scan detection */
    api_response_t *response = api_auth_get_info(auth_data.uid);
    if (!response || response->status_code != 200) {
        api_response_free(response);
        return false;
    }

    cJSON *json = cJSON_Parse(response->data);
    if (!json) {
        api_response_free(response);
        return false;
    }

    /* Check for QR code expiry */
    cJSON *status = cJSON_GetObjectItem(json, "status");
    if (status && status->valueint == 4) {
        snprintf(auth_data.error_msg, sizeof(auth_data.error_msg),
                "二维码已过期，请重新登录");
        auth_data.state = AUTH_STATE_ERROR;
        cJSON_Delete(json);
        api_response_free(response);
        return false;
    }

    /* Check for server error */
    cJSON *err_code = cJSON_GetObjectItem(json, "errCode");
    if (err_code && err_code->valueint != 0 && err_code->valueint != -1) {
        snprintf(auth_data.error_msg, sizeof(auth_data.error_msg),
                "登录错误 (errCode=%d)", err_code->valueint);
        auth_data.state = AUTH_STATE_ERROR;
        cJSON_Delete(json);
        api_response_free(response);
        return false;
    }

    /*
     * Scan detection: require BOTH skey AND vid present.
     * NOTE: vid may be a NUMBER (e.g. 12651723) not a string!
     */
    cJSON *skey = cJSON_GetObjectItem(json, "skey");
    cJSON *vid = cJSON_GetObjectItem(json, "vid");
    cJSON *code = cJSON_GetObjectItem(json, "code");

    char skey_buf[256] = {0}, vid_buf[128] = {0}, code_buf[128] = {0};
    bool has_skey = cjson_get_string(skey, skey_buf, sizeof(skey_buf));
    bool has_vid = cjson_get_string(vid, vid_buf, sizeof(vid_buf));
    bool has_code = cjson_get_string(code, code_buf, sizeof(code_buf));

    /* Log full response for debugging */
    printf("[AUTH] getinfo: %s\n", response->data);
    printf("[AUTH] skey=%s vid=%s code=%s\n",
           has_skey ? skey_buf : "(missing)",
           has_vid ? vid_buf : "(missing)",
           has_code ? code_buf : "(missing)");

    if (has_skey && has_vid) {
        /*
         * Scan detected + phone confirmed (getinfo only returns skey+vid
         * AFTER the user confirms on their phone).
         *
         * JIX UI path skips weblogin entirely and goes straight to
         * session_init, using skey as both accessToken and refreshToken.
         */
        strncpy(auth_data.skey, skey_buf, sizeof(auth_data.skey) - 1);
        strncpy(auth_data.vid, vid_buf, sizeof(auth_data.vid) - 1);
        if (has_code) {
            strncpy(auth_data.code, code_buf, sizeof(auth_data.code) - 1);
        }

        /* Use skey as accessToken and refreshToken for session_init */
        strncpy(auth_data.access_token, skey_buf, sizeof(auth_data.access_token) - 1);
        strncpy(auth_data.refresh_token, skey_buf, sizeof(auth_data.refresh_token) - 1);

        printf("[AUTH] QR scanned! skey+vid obtained, skipping weblogin → session_init\n");
        auth_data.state = AUTH_STATE_SESSION_INIT;
        cJSON_Delete(json);
        api_response_free(response);
        return true;
    }

    /* Still waiting for scan */
    cJSON_Delete(json);
    api_response_free(response);
    return false;
}

bool auth_poll_confirm(void)
{
    if (auth_data.state != AUTH_STATE_WAIT_CONFIRM) {
        return false;
    }

    /*
     * Step 3: Call weblogin with the FULL getinfo response payload.
     * JIX passes the entire getinfo dict (minus some keys + fp="").
     */
    char *payload = build_weblogin_payload(auth_data.getinfo_response);
    if (!payload) {
        printf("[AUTH] Failed to build weblogin payload\n");
        return false;
    }

    printf("[AUTH] weblogin payload: %s\n", payload);
    api_response_t *response = api_auth_web_login_raw(payload);
    free(payload);

    if (!response || response->status_code != 200) {
        printf("[AUTH] weblogin request failed (status=%d)\n",
               response ? response->status_code : -1);
        api_response_free(response);
        return false;
    }

    cJSON *json = cJSON_Parse(response->data);
    if (!json) {
        printf("[AUTH] Failed to parse weblogin response: %s\n", response->data);
        api_response_free(response);
        return false;
    }

    printf("[AUTH] weblogin response: %s\n", response->data);

    /* Check for server error */
    cJSON *err_code = cJSON_GetObjectItem(json, "errCode");
    if (err_code && err_code->valueint != 0) {
        cJSON *err_msg = cJSON_GetObjectItem(json, "errMsg");
        snprintf(auth_data.error_msg, sizeof(auth_data.error_msg),
                "Web登录失败: %s",
                (err_msg && cJSON_IsString(err_msg)) ? err_msg->valuestring : "unknown");
        auth_data.state = AUTH_STATE_ERROR;
        cJSON_Delete(json);
        api_response_free(response);
        return false;
    }

    /* Check if user needs to confirm on phone */
    cJSON *need_verify = cJSON_GetObjectItem(json, "needRemoteLoginVerify");
    if (need_verify && cJSON_IsTrue(need_verify)) {
        printf("[AUTH] Waiting for phone confirmation...\n");
        cJSON_Delete(json);
        api_response_free(response);
        return false;
    }

    cJSON *access_token = cJSON_GetObjectItem(json, "accessToken");
    cJSON *refresh_token = cJSON_GetObjectItem(json, "refreshToken");
    cJSON *resp_vid = cJSON_GetObjectItem(json, "vid");

    if (access_token && cJSON_IsString(access_token) &&
        strlen(access_token->valuestring) > 0) {

        strncpy(auth_data.access_token, access_token->valuestring,
                sizeof(auth_data.access_token) - 1);

        if (refresh_token && cJSON_IsString(refresh_token)) {
            strncpy(auth_data.refresh_token, refresh_token->valuestring,
                    sizeof(auth_data.refresh_token) - 1);
        }
        if (resp_vid && cJSON_IsString(resp_vid)) {
            strncpy(auth_data.vid, resp_vid->valuestring, sizeof(auth_data.vid) - 1);
        }

        printf("[AUTH] User confirmed! accessToken obtained.\n");
        auth_data.state = AUTH_STATE_SESSION_INIT;
        cJSON_Delete(json);
        api_response_free(response);
        return true;
    }

    cJSON_Delete(json);
    api_response_free(response);
    return false;
}

bool auth_complete(void)
{
    if (auth_data.state != AUTH_STATE_SESSION_INIT) {
        return false;
    }

    printf("[AUTH] Initializing session...\n");

    /* Step 4: Session init */
    api_response_t *response = api_auth_session_init(auth_data.vid,
                                                     auth_data.access_token,
                                                     auth_data.refresh_token);
    if (!response || response->status_code != 200) {
        snprintf(auth_data.error_msg, sizeof(auth_data.error_msg),
                "Session init failed");
        auth_data.state = AUTH_STATE_ERROR;
        api_response_free(response);
        return false;
    }

    printf("[AUTH] Session init OK, cookies: %s\n", api_get_cookies());
    api_response_free(response);

    /* Renewal to extend session lifetime (empty cookies are ignored) */
    response = api_auth_renewal("https://weread.qq.com/");
    if (response) {
        printf("[AUTH] Renewal done, cookies: %s\n", api_get_cookies());
        api_response_free(response);
    }

    /* Persist cookies to disk for auto-login next time */
    api_save_cookies();

    printf("[AUTH] Authentication successful!\n");
    auth_data.state = AUTH_STATE_SUCCESS;

    /* Cleanup getinfo response - no longer needed */
    if (auth_data.getinfo_response) {
        free(auth_data.getinfo_response);
        auth_data.getinfo_response = NULL;
    }

    return true;
}

auth_state_t auth_get_state(void)
{
    return auth_data.state;
}

const auth_data_t *auth_get_data(void)
{
    return &auth_data;
}

bool auth_is_authenticated(void)
{
    return auth_data.state == AUTH_STATE_SUCCESS;
}

void auth_clear(void)
{
    if (auth_data.getinfo_response) {
        free(auth_data.getinfo_response);
    }
    memset(&auth_data, 0, sizeof(auth_data));
    auth_data.state = AUTH_STATE_IDLE;
}
