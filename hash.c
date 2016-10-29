/* hash.c: fast hashing primitives for (mainly) natmap.c
 *
 * This uses the final mixing step of xxhash:
 * https://github.com/Cyan4973/xxHash
 */

#ifndef __HASH_C__
#define __HASH_C__

#include <stdint.h>
#include <string.h>
#include <stddef.h>

static inline uint32_t hash_u32_u32(uint32_t x) {
    x = 0x85ebca6b * (x ^ (x >> 16));
    x = 0xc2b2ae35 * (x ^ (x >> 13));
    return x ^ (x >> 16);
}

static inline uint64_t hash_u64_u64(uint64_t x) {
    x = (x ^ (x >> 33)) * 14029467366897019727ULL;
    x = (x ^ (x >> 29)) * 1609587929392839161ULL;
    return x ^ (x >> 32);
}

#endif

