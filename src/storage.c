/*
 * Common functions used in all storages
 */
#include <string.h>
#include <stdlib.h>
#include "storage.h"

inline void * memndup(const void * p, const size_t sz) {
    void * ret = malloc(sz);
    memcpy(ret, p, sz);
    return ret;
}

inline int keycmp(
        const void * key1, const size_t len1,
        const void * key2, const size_t len2) {
    return (len2 == len1)? memcmp(key1, key2, len1): len1 - len2;
}

