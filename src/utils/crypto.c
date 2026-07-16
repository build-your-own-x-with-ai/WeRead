/**
 * WeRead Crypto Utilities Implementation
 *
 * Exact port of JIX weread_cli/client.py crypto functions.
 */

#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <openssl/md5.h>
#include <openssl/evp.h>
#include <openssl/bio.h>

/* ── helpers ──────────────────────────────────────────────────────────── */

static void bytes_to_hex(const unsigned char *bytes, size_t len, char *output)
{
    for (size_t i = 0; i < len; i++) {
        sprintf(output + (i * 2), "%02x", bytes[i]);
    }
}

static void md5_string(const char *input, char *hex_out)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char *)input, strlen(input), digest);
    bytes_to_hex(digest, MD5_DIGEST_LENGTH, hex_out);
}

static int is_digit_string(const char *s)
{
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
    }
    return 1;
}

/* ── calc_hash ────────────────────────────────────────────────────────── */
/*
 * Python reference (client.py:60-83):
 *
 * def calc_hash(value):
 *     data = str(value)
 *     data_md5 = md5(data)
 *     result = data_md5[:3]
 *     if data.isdigit():
 *         chunks = [format(int(data[i:i+9]), "x") for i in range(0,len(data),9)]
 *         kind = "3"
 *     else:
 *         chunks = ["".join(format(ord(ch),"x") for ch in data)]
 *         kind = "4"
 *     result += kind + "2" + data_md5[-2:]
 *     for index, chunk in enumerate(chunks):
 *         length = format(len(chunk), "x").zfill(2)
 *         result += length + chunk
 *         if index < len(chunks) - 1:
 *             result += "g"
 *     if len(result) < 20:
 *         result += data_md5[:20 - len(result)]
 *     return result + md5(result)[:3]
 */

int calc_hash(const char *value, char *output)
{
    if (!value || !output) return -1;

    char data_md5[33] = {0};
    md5_string(value, data_md5);

    char result[128] = {0};
    int rlen = 0;

    /* Prefix: first 3 chars of MD5 */
    result[0] = data_md5[0];
    result[1] = data_md5[1];
    result[2] = data_md5[2];
    rlen = 3;

    char kind;
    char chunks[16][64];
    int chunk_count = 0;

    if (is_digit_string(value)) {
        /* Digit: split into 9-digit chunks, each to hex */
        kind = '3';
        int vlen = (int)strlen(value);
        for (int i = 0; i < vlen; i += 9) {
            int end = i + 9;
            if (end > vlen) end = vlen;
            char chunk_str[10];
            int clen = end - i;
            memcpy(chunk_str, value + i, clen);
            chunk_str[clen] = '\0';
            long long num = atoll(chunk_str);
            snprintf(chunks[chunk_count], sizeof(chunks[0]), "%llx", num);
            chunk_count++;
        }
    } else {
        /* Non-digit: all chars' hex ordinals concatenated into one chunk */
        kind = '4';
        char buf[512] = {0};
        int blen = 0;
        for (const char *p = value; *p; p++) {
            blen += snprintf(buf + blen, sizeof(buf) - blen, "%x",
                             (unsigned char)*p);
        }
        strncpy(chunks[0], buf, sizeof(chunks[0]) - 1);
        chunk_count = 1;
    }

    /* kind + "2" + data_md5[-2:] */
    result[rlen++] = kind;
    result[rlen++] = '2';
    result[rlen++] = data_md5[30];
    result[rlen++] = data_md5[31];

    /* Length-prefixed chunks separated by "g" */
    for (int i = 0; i < chunk_count; i++) {
        int chunk_len = (int)strlen(chunks[i]);
        rlen += snprintf(result + rlen, sizeof(result) - rlen, "%02x", chunk_len);
        rlen += snprintf(result + rlen, sizeof(result) - rlen, "%s", chunks[i]);
        if (i < chunk_count - 1) {
            result[rlen++] = 'g';
        }
    }

    /* Pad to at least 20 chars using MD5 prefix */
    if (rlen < 20) {
        int needed = 20 - rlen;
        memcpy(result + rlen, data_md5, needed);
        rlen += needed;
    }

    /* Append first 3 chars of MD5(result so far) */
    result[rlen] = '\0';
    char result_md5[33] = {0};
    md5_string(result, result_md5);
    result[rlen++] = result_md5[0];
    result[rlen++] = result_md5[1];
    result[rlen++] = result_md5[2];
    result[rlen] = '\0';

    strcpy(output, result);
    return 0;
}

/* ── url_encode ───────────────────────────────────────────────────────── */
/* Matches Python urllib.parse.quote(s, safe='') */

int url_encode(const char *input, char *output, size_t output_size)
{
    if (!input || !output) return -1;

    size_t pos = 0;
    for (size_t i = 0; input[i] && pos < output_size - 4; i++) {
        char c = input[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            output[pos++] = c;
        } else {
            snprintf(output + pos, output_size - pos, "%%%02X",
                     (unsigned char)c);
            pos += 3;
        }
    }
    output[pos] = '\0';
    return 0;
}

/* ── sign_payload ─────────────────────────────────────────────────────── */
/*
 * Python reference (client.py:86-99):
 *
 * def sign_payload(data):
 *     items = [f"{k}={quote(str(data[k]), safe='')}" for k in sorted(data)]
 *     raw = "&".join(items)
 *     n1 = 0x15051505
 *     n2 = 0x15051505
 *     length = len(raw)
 *     index = length - 1
 *     while index > 0:
 *         n1 = 0x7FFFFFFF & (n1 ^ (ord(raw[index])   << ((length - index) % 30)))
 *         n2 = 0x7FFFFFFF & (n2 ^ (ord(raw[index-1]) << (index % 30)))
 *         index -= 2
 *     return format(n1 + n2, "x").lower()
 */

static int compare_strings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

int sign_payload(const char **keys, const char **values, int count, char *output)
{
    if (!keys || !values || !output || count <= 0) return -1;

    /* Build index array sorted by key name */
    int *sorted_indices = malloc(count * sizeof(int));
    const char **sorted_keys = malloc(count * sizeof(char *));
    if (!sorted_indices || !sorted_keys) {
        free(sorted_indices);
        free(sorted_keys);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        sorted_indices[i] = i;
        sorted_keys[i] = keys[i];
    }
    qsort(sorted_keys, count, sizeof(char *), compare_strings);

    /* Build URL-encoded query: key1=val1&key2=val2&... */
    char query[8192] = {0};
    int qlen = 0;

    for (int i = 0; i < count; i++) {
        /* Find original index for this sorted key */
        int idx = 0;
        for (int j = 0; j < count; j++) {
            if (strcmp(sorted_keys[i], keys[j]) == 0) {
                idx = j;
                break;
            }
        }

        if (i > 0) query[qlen++] = '&';
        qlen += snprintf(query + qlen, sizeof(query) - qlen, "%s=",
                         sorted_keys[i]);

        char encoded[2048];
        url_encode(values[idx], encoded, sizeof(encoded));
        qlen += snprintf(query + qlen, sizeof(query) - qlen, "%s", encoded);
    }

    /* XOR-shift hash: iterate backward in steps of 2 */
    uint32_t n1 = 0x15051505;
    uint32_t n2 = 0x15051505;
    int length = qlen;
    int index = length - 1;

    while (index > 0) {
        uint32_t shift1 = (uint32_t)((length - index) % 30);
        uint32_t shift2 = (uint32_t)(index % 30);
        n1 = 0x7FFFFFFF & (n1 ^ ((uint32_t)(unsigned char)query[index] << shift1));
        n2 = 0x7FFFFFFF & (n2 ^ ((uint32_t)(unsigned char)query[index - 1] << shift2));
        index -= 2;
    }

    uint32_t result = n1 + n2;
    snprintf(output, 65, "%x", result);

    free(sorted_indices);
    free(sorted_keys);
    return 0;
}

/* ── decrypt_wr ───────────────────────────────────────────────────────── */
/*
 * Python reference (client.py:102-133):
 *
 * def decrypt_wr(data):
 *     result = data[1:]
 *     length = len(result)
 *     if length >= 11:
 *         tail_len = min(4, ceil(length/10))
 *         bits = ""
 *         for index in range(length-1, length-1-tail_len, -1):  ← RIGHT to LEFT
 *             bits += str(int(format(ord(result[index]), "b"), 4))
 *         ...
 */

int decrypt_wr(const char *encrypted, char *output, size_t output_size)
{
    if (!encrypted || !output) return -1;

    size_t len = strlen(encrypted);
    if (len <= 1) {
        output[0] = '\0';
        return 0;
    }

    /* Strip first character */
    const char *data = encrypted + 1;
    len--;

    if (len == 0) {
        output[0] = '\0';
        return 0;
    }

    char *chars = strdup(data);
    if (!chars) return -1;

    /* Compute swap positions from tail characters */
    int positions[20];
    int pos_count = 0;

    if (len < 4) {
        pos_count = 0;
    } else if (len < 11) {
        positions[0] = 0;
        positions[1] = 2;
        pos_count = 2;
    } else {
        int tail_len = (int)((len + 9) / 10); /* ceil(len/10) */
        if (tail_len > 4) tail_len = 4;

        /*
         * Build bit string from tail characters, reading RIGHT to LEFT.
         * Python: for index in range(length-1, length-1-tail_len, -1)
         *         bits += format(ord(result[index]), 'b').zfill(4)
         *
         * Each character's full binary representation (zfilled to 4 bits).
         * For base64 chars (ASCII 43-122), binary is 6-7 bits.
         */
        char bits[256] = {0};
        int bits_len = 0;

        for (int i = 0; i < tail_len; i++) {
            int char_idx = (int)len - 1 - i; /* right to left */
            unsigned int code = (unsigned char)chars[char_idx];

            /* Convert to binary string */
            char bin_str[33];
            int bin_len = 0;
            if (code == 0) {
                bin_str[0] = '0';
                bin_len = 1;
            } else {
                char tmp[33];
                int tmp_len = 0;
                unsigned int n = code;
                while (n > 0) {
                    tmp[tmp_len++] = (n & 1) ? '1' : '0';
                    n >>= 1;
                }
                /* Reverse to get MSB first */
                for (int k = tmp_len - 1; k >= 0; k--) {
                    bin_str[bin_len++] = tmp[k];
                }
            }

            /* Zero-pad to at least 4 bits */
            while (bin_len < 4) {
                memmove(bin_str + 1, bin_str, bin_len + 1);
                bin_str[0] = '0';
                bin_len++;
            }
            bin_str[bin_len] = '\0';

            strcat(bits + bits_len, bin_str);
            bits_len += bin_len;
        }

        int mod_base = (int)len - tail_len - 2;
        if (mod_base <= 0) mod_base = 1;

        char mod_str[32];
        snprintf(mod_str, sizeof(mod_str), "%d", mod_base);
        int mod_len = (int)strlen(mod_str);

        /*
         * Read overlapping position pairs from the bits string.
         * Python:
         *   num = int(bits[index : index + mod_len])
         *   positions.append(num % mod_base)
         *   num = int(bits[index+1 : index+1 + mod_len])
         *   positions.append(num % mod_base)
         *   index += mod_len
         */
        int idx = 0;
        while (pos_count < 10 && idx + mod_len <= bits_len) {
            int val1 = 0;
            for (int k = 0; k < mod_len && idx + k < bits_len; k++) {
                val1 = val1 * 10 + (bits[idx + k] - '0');
            }
            positions[pos_count++] = val1 % mod_base;

            if (idx + 1 + mod_len <= bits_len) {
                int val2 = 0;
                for (int k = 0; k < mod_len && idx + 1 + k < bits_len; k++) {
                    val2 = val2 * 10 + (bits[idx + 1 + k] - '0');
                }
                positions[pos_count++] = val2 % mod_base;
            }

            idx += mod_len;
        }
    }

    /* Perform paired character swaps in reverse order */
    for (int i = pos_count - 1; i >= 1; i -= 2) {
        for (int j = 1; j >= 0; j--) {
            int a = positions[i] + j;
            int b = positions[i - 1] + j;
            if (a >= 0 && a < (int)len && b >= 0 && b < (int)len) {
                char t = chars[a];
                chars[a] = chars[b];
                chars[b] = t;
            }
        }
    }

    /* Base64 decode */
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new_mem_buf(chars, (int)len);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    int decode_len = BIO_read(bio, output, (int)(output_size - 1));
    if (decode_len < 0) decode_len = 0;
    output[decode_len] = '\0';

    BIO_free_all(bio);
    free(chars);

    return decode_len;
}

/* ── chk ──────────────────────────────────────────────────────────────── */
/*
 * Python reference (client.py:136-141):
 *
 * def chk(data):
 *     if not data or len(data) <= 32:
 *         return data
 *     header = data[:32]
 *     body = data[32:]
 *     return body if header == md5(body).upper() else ""
 */

int chk(const char *data, const char *expected_md5)
{
    (void)expected_md5; /* JIX chk() is self-verifying */

    if (!data) return 0;

    size_t len = strlen(data);
    if (len <= 32) return 0;

    char header[33] = {0};
    strncpy(header, data, 32);

    const char *body = data + 32;
    size_t body_len = len - 32;

    unsigned char md5_digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char *)body, body_len, md5_digest);

    char md5_hex[33] = {0};
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(md5_hex + (i * 2), "%02X", md5_digest[i]);
    }

    return strcmp(header, md5_hex) == 0;
}

const char *chk_body(const char *data)
{
    if (!data) return NULL;
    size_t len = strlen(data);
    if (len <= 32) return data;
    return data + 32;
}
