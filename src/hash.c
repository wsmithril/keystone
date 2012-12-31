/*
 * Set of Hash functions
 */

#include <string.h>
#include <stdint.h>
#include "hash.h"

// endians
#if defined(__sparc) || defined(__sparc__) \
    || defined(_POWER) || defined(__powerpc__) \
    || defined(__ppc__) || defined(__hpux) || defined(__hppa) \
    || defined(_MIPSEB) || defined(_POWER) \
    || defined(__s390__)
#   define WORDS_BIGENDIAN
#elif defined(__i386__) || defined(__alpha__) \
    || defined(__ia64) || defined(__ia64__) \
    || defined(_M_IX86) || defined(_M_IA64) \
    || defined(_M_ALPHA) || defined(__amd64) \
    || defined(__amd64__) || defined(_M_AMD64) \
    || defined(__x86_64) || defined(__x86_64__) \
    || defined(_M_X64) || defined(__bfin__)
#   define WORDS_LITTLEENDIAN
#endif

#include <byteswap.h>
#if !defined(WORDS_BIGENDIAN)
#   define ordered(x) (x)
#else
#   define ordered(x) (bswap_64(x))
#endif // WORDS_BIGENDIAN

static inline
uint32_t rotl32(uint32_t x, int8_t r) {
  return (x << r) | (x >> (32 - r));
}

static inline
uint64_t rotl64(uint64_t x, int8_t r) {
  return (x << r) | (x >> (64 - r));
}

#define ROTL32(x,y)     rotl32(x,y)
#define ROTL64(x,y)     rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

#define getblock(b, i) (ordered(b[i]))
#define FORCE_INLINE __attribute__((always_inline))

#define fmix(k) do { \
  k ^= k >> 33; \
  k *= BIG_CONSTANT(0xff51afd7ed558ccd); \
  k ^= k >> 33; \
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53); \
  k ^= k >> 33; \
} while(0)

uint64_t murmurhash(const void * key, const size_t len) {
    const uint8_t  * data   = (const uint8_t  *)(key);
    const uint64_t * blocks = (const uint64_t *)(data);
    const int nblocks = len / 16;

    const uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
    const uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);
    uint64_t h1 = 0xc949d7c7509e6557ULL;
    uint64_t h2 = 0xc949d7c7509e6557ULL;

    int i = 0;
    for (i = 0; i < nblocks; i++) {
        uint64_t k1 = getblock(blocks, i * 2 + 0);
        uint64_t k2 = getblock(blocks, i * 2 + 1);

        k1 *= c1;
        k1  = ROTL64(k1, 31);
        k1 *= c2;
        h1 ^= k1;
        h1  = ROTL64(h1, 27);
        h1 += h2;
        h1  = h1 * 5 + 0x52dce729;
        k2 *= c2;
        k2  = ROTL64(k2, 33);
        k2 *= c1;
        h2 ^= k2;
        h2  = ROTL64(h2, 31);
        h2 += h1;
        h2  = h2 * 5 + 0x38495ab5;
    }

    const uint8_t * tail = (const uint8_t*)(data + nblocks * 16);

    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch (len & 15) {
        case 15: k2 ^= (uint64_t)(tail[14]) << 48;
        case 14: k2 ^= (uint64_t)(tail[13]) << 40;
        case 13: k2 ^= (uint64_t)(tail[12]) << 32;
        case 12: k2 ^= (uint64_t)(tail[11]) << 24;
        case 11: k2 ^= (uint64_t)(tail[10]) << 16;
        case 10: k2 ^= (uint64_t)(tail[ 9]) << 8;
        case  9: k2 ^= (uint64_t)(tail[ 8]) << 0;
                 k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

        case  8: k1 ^= (uint64_t)(tail[ 7]) << 56;
        case  7: k1 ^= (uint64_t)(tail[ 6]) << 48;
        case  6: k1 ^= (uint64_t)(tail[ 5]) << 40;
        case  5: k1 ^= (uint64_t)(tail[ 4]) << 32;
        case  4: k1 ^= (uint64_t)(tail[ 3]) << 24;
        case  3: k1 ^= (uint64_t)(tail[ 2]) << 16;
        case  2: k1 ^= (uint64_t)(tail[ 1]) << 8;
        case  1: k1 ^= (uint64_t)(tail[ 0]) << 0;
                 k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
    };

    h1 ^= len; h2 ^= len;
    h1 += h2;
    h2 += h1;
    fmix(h1);
    fmix(h2);
    h1 += h2;
    h2 += h1;
    return h1 ^ h2;
}

