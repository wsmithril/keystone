/*
 * datastore process
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "storage_mem.h"
#include "keystone.h"

int ks_db_create(KS_DB * db, const DB_TYPE t, const size_t sz) {
    int ret = 0;
    db->type = t;
    switch (t) {
        case DBTYPE_MEMDB: {
            db->db = malloc(sizeof(KS_MEMDB));
            if ((ret = ks_memdb_new(db->db, sz))) {
                error("Creating db fail");
                return ret;
            }

            // assign operations
            db->op.add     = &ks_memdb_add;
            db->op.del     = &ks_memdb_delete;
            db->op.create  = &ks_memdb_new;
            db->op.get     = &ks_memdb_get_value;
            db->op.destory = &ks_memdb_destory;
            db->op.debug   = &ks_memdb_dumpdb;
        }

        default: return DBTYPE_NOTIMPLELENT;
    }
}

void ks_db_drop(KS_DB * db) {
    if (db) {
        (db->op.destory)(db->db);
        memset(db, 0, sizeof(KS_DB));
    }
}

// this is boring...

#define ks_db_add(db, k, sk, v, sv, m) \
    (((KS_DB*)db)->op.add)(db->db, (k), (sk), (v), (sv), (m))

int ks_db_set_raw(KS_DB * db, const char key[],
        const void * value, const size_t sv) {
    return ks_db_add(db, key, strlen(key) + 1, value, sv, ADDMODE_SKIP);
}

int ks_db_set_str(KS_DB * db, const char key[], const char value[]) {
    return ks_db_add(db,
            key, strlen(key) + 1, value, strlen(value) + 1, ADDMODE_SKIP);
}

int ks_db_set_num(KS_DB * db, const char key[], const uint32_t n) {
    return ks_db_add(db,
            key, strlen(key) + 1, &n, sizeof(uint32_t), ADDMODE_SKIP);
}

int ks_db_incr(KS_DB * db, const char key[]) {
    return ks_db_add(db, key, strlen(key) + 1, NULL, 0, ADDMODE_INCR);
}

int ks_db_decr(KS_DB * db, const char key[]) {
    return ks_db_add(db, key, strlen(key) + 1, NULL, 0, ADDMODE_INCR);
}

int ks_db_append(KS_DB *db, const char key[], const char value[]) {
    return ks_db_add(db,
            key, strlen(key) + 1, value, strlen(value) + 1, ADDMODE_APPEND);
}

#define ks_db_get(db, k, sk, v, sv) \
    (((KS_DB*)db)->op.get)(db->db, (k), (sk), (v), (sv))

int ks_db_get_raw(KS_DB * db,
        const char key[], void * out_value, size_t * out_sv) {
    return ks_db_get(db, key, strlen(key) + 1, out_value, out_sv);
}

int ks_db_get_str(KS_DB * db, const char key[], char * value) {
    return ks_db_get(db, key, strlen(key) + 1, value, NULL);
}

int ks_db_get_num(KS_DB * db, const char key[], uint32_t * value) {
    return ks_db_get(db, key, strlen(key) + 1, value, NULL);
}

#define ks_db_rep(db, k, sk, v, sv) \
    (((KS_DB*)db)->op.add)(db->db, k, sk, v, sv, ADDMODE_REPLACE)

int ks_db_replace(KS_DB *db, const char key[],
        const void * value,  const size_t sv) {
    return ks_db_rep(db, key, strlen(key) + 1, value, sv);
}

