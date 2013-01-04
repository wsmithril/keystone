#ifndef _KEY_STONE_H_
#define _KEY_STONE_H_

#include <stdint.h>
#include <stdlib.h>
#include "storage.h"

#ifndef OK
#   define OK 0
#endif

#define DBTYPE_NOTIMPLELENT 1

enum DB_TYPE {
    DBTYPE_MEMDB = 0,
};
typedef enum DB_TYPE DB_TYPE;

struct KS_DB {
    DB_TYPE type;
    uint32_t lock;
    void * db;
    struct {
        int  (*add)(void *, const void *, const size_t, const void *,
                const size_t, const KS_ADD_MODE);
        int  (*del)(void *, const void *, const size_t);
        int  (*create)(void *, const uint64_t);
        void (*destory)(void *);
        int  (*get)(void *, const void *, const size_t,
                void *, size_t *);
        void (*debug)(const void *);
    } op;
};
typedef struct KS_DB KS_DB;

// iterators
struct KS_DB_ITER {
    struct {
        int (*cerate)(void *, void *);
        int (*next)(void *);
        int (*destory)(void *);
        int (*tell)(void *, void *, size_t *, void *, size_t *);
    } op;
    KS_DB * db;
    void * record;
}; // iterator for db, unsafe

// boring part...
// done with g/\(void\|int\) ks_db_\([^)]\|\n\)*) {$/y A

void ks_db_drop(KS_DB * db);
int ks_db_create(KS_DB * db, const DB_TYPE t, const size_t sz);
int ks_db_set_raw(KS_DB * db, const char key[],
        const void * value, const size_t sv);
int ks_db_set_str(KS_DB * db, const char key[], const char value[]);
int ks_db_set_num(KS_DB * db, const char key[], const uint32_t n);
int ks_db_incr(KS_DB * db, const char key[]);
int ks_db_decr(KS_DB * db, const char key[]);
int ks_db_append(KS_DB *db, const char key[], const char value[]);
int ks_db_get_raw(KS_DB * db,
        const char key[], void * out_value, size_t * out_sv);
int ks_db_get_str(KS_DB * db, const char key[], char * value);
int ks_db_get_num(KS_DB * db, const char key[], uint32_t * value);
int ks_db_replace(KS_DB *db, const char key[],
        const void * value,  const size_t sv);


#endif /* _KEY_STONE_H_ */
