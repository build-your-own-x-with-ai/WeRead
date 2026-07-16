/**
 * WeRead Authentication Controller
 *
 * Manages the 6-step QR code login flow
 */

#ifndef AUTH_CONTROLLER_H
#define AUTH_CONTROLLER_H

#include <stdbool.h>

/* Authentication state */
typedef enum {
    AUTH_STATE_IDLE,
    AUTH_STATE_GET_UID,
    AUTH_STATE_WAIT_SCAN,
    AUTH_STATE_WAIT_CONFIRM,
    AUTH_STATE_SESSION_INIT,
    AUTH_STATE_SUCCESS,
    AUTH_STATE_ERROR
} auth_state_t;

/* Authentication data */
typedef struct {
    char uid[128];
    char qr_url[512];
    char skey[256];
    char vid[128];
    char code[128];
    char access_token[256];
    char refresh_token[256];
    auth_state_t state;
    char error_msg[256];
    char *getinfo_response;  /* Full getinfo JSON to pass to weblogin */
} auth_data_t;

/**
 * Initialize authentication controller
 */
void auth_init(void);

/**
 * Start authentication flow
 * Step 1: Get UID and QR URL
 */
bool auth_start(void);

/**
 * Poll for scan status
 * Step 2: Check if user scanned QR code
 * Returns: true if user scanned, false if still waiting
 */
bool auth_poll_scan(void);

/**
 * Poll for confirmation
 * Step 3: Check if user confirmed on phone
 * Returns: true if confirmed, false if still waiting
 */
bool auth_poll_confirm(void);

/**
 * Complete authentication
 * Step 4-6: Session init and renewal
 */
bool auth_complete(void);

/**
 * Get current authentication state
 */
auth_state_t auth_get_state(void);

/**
 * Get authentication data (UID, QR URL, etc.)
 */
const auth_data_t *auth_get_data(void);

/**
 * Check if authenticated
 */
bool auth_is_authenticated(void);

/**
 * Clear authentication data
 */
void auth_clear(void);

#endif /* AUTH_CONTROLLER_H */
