/*
 * in mem database
 */

#ifndef _KS_STORAGE_MEM_H
#define _KS_STORAGE_MEM_H

// errors
#define OK      0
#define NOMEM   1
#define MEMFULL 2
#define EXTEND_ERR  3

#define ADD_SKIPPED   4
#define NOT_FOUND     5
#define APPEND_NONSTR 6
#define INCR_NAN      7
#include <stdint.h>

#define MEMDB_DEFAULT 1024

#ifndef KS_MEMDB_THR_FILL
#   define KS_MEMDB_THR_FILL 2.5
#endif

struct KS_MEMDB_REC {
    size_t sk;
    size_t sv;
    uint32_t flags; // magic happens here, careful with this
    void * key;
    void * value;
    struct KS_MEMDB_REC * next;
};
typedef struct KS_MEMDB_REC KS_MEMDB_REC;

enum KS_MEMDB_ADD_MODE {
    ADDMODE_NOOP = 0,
    ADDMODE_APPEND,
    ADDMODE_REPLACE,
    ADDMODE_SKIP,
    ADDMODE_INCR,
};
typedef enum KS_MEMDB_ADD_MODE KS_MEMDB_ADD_MODE;

struct KS_MEMDB {
    uint64_t size;
    uint64_t used[2];
    uint64_t redirect_idx;
    uint64_t (* hash_func)(const void *, size_t);
    uint32_t flags;         // magic happens here, careful with this
    KS_MEMDB_ADD_MODE mode; // default behavior when key exisits
    KS_MEMDB_REC ** hash[2];
    uint8_t  * slot_lock[2];
};
typedef struct KS_MEMDB KS_MEMDB;

int ks_memdb_add(void * in_db,
        const void * key,   const size_t sk,
        const void * value, const size_t sv,
        const KS_MEMDB_ADD_MODE);

int ks_memdb_new(void * in_db, const uint32_t size);

void ks_memdb_destory(void * in_db);

int ks_memdb_delete(void * in_db, const void * key, const size_t sk);

void ks_memdb_dumpdb(const void * in_db);

int ks_memdb_get_value(void * db,
        const char * key, const size_t sk,
        char * out_value, size_t * out_sv);

#endif /* _KS_STORAGE_MEM_H */
