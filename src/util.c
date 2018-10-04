#include "util.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

/// Generates a random integer from [mn, mx)
u64 randint(u64 mn, u64 mx) {
    return mn + rand() / (RAND_MAX / (mx - mn) + 1);
}

/*
 * Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/> -- 2005-03-30
 * License: http://www.cl.cam.ac.uk/~mgk25/short-license.html
 *
 * Modified by dht author, 2018.
 */
bool is_valid_utf8(const unsigned char s[], u64 maxlen) {
    u64 ix = 0;
    while (ix < maxlen) {
        if (s[ix] < 0x80)
            /* 0xxxxxxx */
            ix++;
        else if ((ix < maxlen - 1) && (s[ix] & 0xe0) == 0xc0) {
            /* 110XXXXx 10xxxxxx */
            if ((s[ix + 1] & 0xc0) != 0x80 || (s[ix] & 0xfe) == 0xc0) {
                /* overlong? */
                return false;
            } else {
                ix += 2;
            }
        } else if ((ix < maxlen - 2) && (s[ix] & 0xf0) == 0xe0) {
            /* 1110XXXX 10Xxxxxx 10xxxxxx */
            if ((s[ix + 1] & 0xc0) != 0x80 || (s[ix + 2] & 0xc0) != 0x80 ||
                (s[ix] == 0xe0 && (s[ix + 1] & 0xe0) == 0x80) || /* overlong? */
                (s[ix] == 0xed &&
                 (s[ix + 1] & 0xe0) == 0xa0) || /* surrogate? */
                (s[ix] == 0xef && s[ix + 1] == 0xbf &&
                 (s[ix + 2] & 0xfe) == 0xbe)) /* U+FFFE or U+FFFF? */ {
                return false;
            } else {
                ix += 3;
            }
        } else if ((ix < maxlen - 3) && (s[ix] & 0xf8) == 0xf0) {
            /* 11110XXX 10XXxxxx 10xxxxxx 10xxxxxx */
            if ((s[ix + 1] & 0xc0) != 0x80 || (s[ix + 2] & 0xc0) != 0x80 ||
                (s[ix + 3] & 0xc0) != 0x80 ||
                (s[ix] == 0xf0 && (s[ix + 1] & 0xf0) == 0x80) || /* overlong? */
                (s[ix] == 0xf4 && s[ix + 1] > 0x8f) ||
                s[ix] > 0xf4) /* > U+10FFFF? */
                return false;
            else
                ix += 4;
        } else {
            return false;
        }
    }

    return true;
}

// Maps u8 to the position of its highest set bit
static const u8 g_dkad_tab[256] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

/// Takes the log kademlia distance between two sha1 hashes.
inline u8 dkad(const char x[20], const char y[20]) {

    u8 d = 160;

    for (int i = 0; i < 20; i++) {
        u8 xor = x[i] ^ y[i];
        if (xor == 0) {
            d -= 8;
            continue;
        } else {
            return d - g_dkad_tab[xor];
        }
    }
    return 0;
}
