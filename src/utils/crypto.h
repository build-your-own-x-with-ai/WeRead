/**
 * WeRead Crypto Utilities
 *
 * Implements WeRead-specific cryptographic functions:
 * - calc_hash: Custom hash function for bookId/chapterUid
 * - sign_payload: XOR-based request signing
 * - decrypt_wr: Chapter content decryption
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

/**
 * Calculate WeRead-specific hash for bookId or chapterUid
 *
 * Algorithm (matches JIX client.py calc_hash):
 * 1. MD5 the input value
 * 2. Take first 3 chars of hex digest as prefix
 * 3. Auto-detect kind: '3' for digit strings, '4' for non-digit
 * 4. Append kind + "2" + last 2 chars of MD5
 * 5. Append length-prefixed hex chunks separated by "g"
 * 6. Pad to at least 20 chars using MD5 prefix
 * 7. Append first 3 chars of MD5(result)
 *
 * @param value Input string (bookId or chapterUid)
 * @param output Buffer for output hash (at least 32 bytes)
 * @return 0 on success, -1 on error
 */
int calc_hash(const char *value, char *output);

/**
 * Sign API request payload
 *
 * Algorithm:
 * 1. Sort dictionary keys alphabetically
 * 2. URL-encode values
 * 3. Concatenate as key1=val1&key2=val2
 * 4. XOR operations with magic numbers
 * 5. Return hex signature
 *
 * @param params Array of key-value pairs
 * @param count Number of parameters
 * @param output Buffer for signature (at least 65 bytes)
 * @return 0 on success, -1 on error
 */
int sign_payload(const char **keys, const char **values, int count, char *output);

/**
 * Decrypt WeRead chapter content
 *
 * Algorithm:
 * 1. Strip first character
 * 2. Extract position list from tail bits
 * 3. Perform character position swapping
 * 4. Base64 decode
 *
 * @param encrypted Encrypted content string
 * @param output Buffer for decrypted content
 * @param output_size Size of output buffer
 * @return Length of decrypted content, or -1 on error
 */
int decrypt_wr(const char *encrypted, char *output, size_t output_size);

/**
 * MD5 checksum validation
 *
 * @param data Input data
 * @param expected_md5 Expected MD5 hash (32 hex chars) - ignored, self-verifying
 * @return 1 if valid, 0 if invalid
 */
int chk(const char *data, const char *expected_md5);

/**
 * Get body from chk-validated data (strips 32-char MD5 header)
 *
 * @param data Input data (with 32-char MD5 header)
 * @return Pointer to body (data + 32), or data itself if too short
 */
const char *chk_body(const char *data);

/**
 * URL encode a string
 *
 * @param input Input string
 * @param output Buffer for encoded output
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int url_encode(const char *input, char *output, size_t output_size);

#endif /* CRYPTO_H */
