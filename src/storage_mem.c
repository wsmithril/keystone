/*
 * in mem database
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "storage_mem.h"
#include "log.h"
#include "hash.h"

#define MDB_REHASH 0x01
#define rehashing(db) (((KS_MEMDB*)db)->flags & MDB_REHASH)
#define set_rehash(db)   do { ((KS_MEMDB*)db)->flags |=  MDB_REHASH; } while(0)
#define unset_rehash(db) do { ((KS_MEMDB*)db)->flags &= ~MDB_REHASH; } while(0)

#define slot_locked(db, t, idx) (((KS_MEMDB*)db)->slot_lock[t][idx / 8] & (1U << (idx % 8)))
#define lock_slot(db, t, idx)   do { ((KS_MEMDB*)db)->slot_lock[t][idx / 8] |=  (1U << (idx % 8)); } while(0)
#define unlock_slot(db, t, idx) do { ((KS_MEMDB*)db)->slot_lock[t][idx / 8] &= ~(1U << (idx % 8)); } while(0)

#define MDB_RECLOCK ((uint32_t)0x01)
#define record_locked(rec)  (((KS_MEMDB_REC*)rec)->flags & MDB_RECLOCK)
#define lock_record(rec)    do { ((KS_MEMDB_REC*)rec)->flags |=  MDB_RECLOCK; } while(0)
#define unlock_record(rec)  do { ((KS_MEMDB_REC*)rec)->flags &= ~MDB_RECLOCK; } while(0)

#define ks_memdb_rehash_step(db) do {\
    if (rehashing(db)) ks_memdb_rehash_one(db); \
} while(0)

#define wait_and_lock_slot(db, t, idx) do { \
    while (slot_locked(db, t, idx)); \
    lock_slot(db, t, idx); \
} while(0)

#define wait_and_lock_record(rec) do { \
    while (record_locked(rec)); \
    lock_record(rec); \
} while(0)

// ================== STITIC METHODS ========================

static void ks_memdb_rehash_one(void * db);
static int ks_memdb_extend(void * db);
static int ks_memdb_need_rehash(void * db);
static void * memndup(const void * p, const size_t sz);
static KS_MEMDB_REC * ks_memdb_lookup(void * in_db,
        const char * key, const size_t sk);

// ================ Now, to bussiness ================

#define round_up(n, r) (((n) + (r) - 1) / (r))
#define round2_64(v) do { \
    v --; \
    v |= v >> 1; \
    v |= v >> 2; \
    v |= v >> 4; \
    v |= v >> 8; \
    v |= v >> 16;\
    v |= v >> 32; \
    v ++; \
} while (0)

__attribute__((always_inline))
static void * memndup(const void * p, const size_t sz) {
    void * ret = malloc(sz);
    memcpy(ret, p, sz);
    return ret;
}

#define HASH(key, len) (*db->hash_func)((const void *)key, len)
static __attribute__((always_inline)) int keycmp(
        const void * key1, const size_t len1,
        const void * key2, const size_t len2) {
    return (len2 == len1)? memcmp(key1, key2, len1): 0;
}

int ks_memdb_new(void * in_db, const uint32_t size) {
    uint64_t init_size = size? size: MEMDB_DEFAULT;
    KS_MEMDB * db = in_db;
    assert(db);
    memset(db, 0, sizeof(KS_MEMDB));

    // round up the to power of 2
    round2_64(init_size);

    debug("Creating database of size %lu, given size: %lu", init_size, size);
    db->hash[0] = (KS_MEMDB_REC ** )
        malloc(sizeof(KS_MEMDB_REC * ) * init_size);

    if (!db->hash[0]) {
        error("Create memdb fail, no more memory");
        return NOMEM;
    }

    db->slot_lock[0] = (uint8_t *)malloc(sizeof(uint8_t) * ((init_size + 7) / 8));

    memset(db->hash[0], 0, sizeof(KS_MEMDB_REC*) * init_size);
    memset(db->slot_lock[0], 0, sizeof(uint8_t) * ((init_size + 7) / 8));

    // second ht and slot_lock holds nothing
    db->hash[1]      = NULL;
    db->slot_lock[1] = NULL;
    db->flags        = 0;
    db->hash_func = &murmurhash;

    db->size = init_size;
    return OK;
}

static KS_MEMDB_REC * ks_memdb_lookup(void * in_db,
        const char * key, const size_t sk) {
    KS_MEMDB * db = in_db;
    uint64_t hash = HASH(key, sk);
    KS_MEMDB_REC * hr = NULL;
    int i = 0;

    // try two hash table, 2 here is not a magic
    for (i = 0; !hr && i < 2 && db->hash[i]; i++) {
        for (hr = (db->hash[i])[hash % (db->size << i)]; hr; hr = hr->next) {
            if (keycmp(hr->key, hr->sk, key, sk) == 0) break;
        }
    }

    return hr;
}

static void ks_memdb_rehash_one(void * in_db) {
    KS_MEMDB * db = in_db;
    KS_MEMDB_REC * hr = NULL, * temp = NULL;
    uint64_t hash = 0;

    // rehash records in one hash slots
    if (db->used[0] == 0) {
        // no more keys in first hash table, free this one
        free(db->hash[0]);
        db->hash[0] = db->hash[1];
        db->hash[1] = NULL;
        db->size *= 2;
        db->used[0] = db->used[1];
        db->used[1] = 0;
        free(db->slot_lock[0]);
        db->slot_lock[0] = db->slot_lock[1];
        db->slot_lock[1] = NULL;
        unset_rehash(db);
        notice("Database rehashing done, current size %d.", db->size);
        return;
    }

    // get next slot in first hash table
    while (!db->hash[0][db->redirect_idx]) {
        db->redirect_idx ++;
        if (db->redirect_idx >= db->size) {
            if (db->used[0] == 0) return; // avoid possible deadlock
            db->redirect_idx = 0;
        }
    }

    debug("Rehashing record %ld, %d done, %d left",
            db->redirect_idx, db->used[1], db->used[0]);

    // lock hash slot in firest table [=[
    wait_and_lock_slot(db, 0, db->redirect_idx);

    hr = db->hash[0][db->redirect_idx];
    while (hr) {
        temp = hr->next;
        hash = HASH(hr->key, hr->sk) % (db->size << 1);
        wait_and_lock_slot(db, 1, hash);
        hr->next = db->hash[1][hash];
        db->hash[1][hash] = hr;
        unlock_slot(db, 1, hash);
        db->used[1]++;
        db->used[0]--;
        hr = temp;
    }

    // ]=]
    db->hash[0][db->redirect_idx] = NULL;
    unlock_slot(db, 0, db->redirect_idx);
    return;
}

static int ks_memdb_need_rehash(void * in_db) {
    KS_MEMDB * db = in_db;
    if (rehashing(db)) return OK; // already rehashing
    if (db->used[0] * 1.0 / db->size > KS_MEMDB_THR_FILL) {
        if (ks_memdb_extend(db)) {
            warnning("Extend db fail, size %d", db->size);
            return EXTEND_ERR;
        }

        debug("Database needs rehashing, size: %d, used %d",
                db->size, db->used[0]);

        db->redirect_idx = 0;
        set_rehash(db);
    }
    return OK;
}

static int ks_memdb_extend(void * in_db) {
    KS_MEMDB * db = in_db;
    if (db->hash[1]) return EXTEND_ERR;
    debug("extending hash table %x, orig size: %d", db, db->size);

    db->hash[1] = (KS_MEMDB_REC ** )
        malloc(sizeof(KS_MEMDB_REC *) * 2 * db->size);
    db->slot_lock[1] = (uint8_t *) malloc(sizeof(uint8_t) * round_up(db->size * 2, 8));

    if (!db->hash[1] || !db->slot_lock[1]) return NOMEM;

    memset(db->hash[1], 0, sizeof(KS_MEMDB_REC *) * 2 * db->size);
    memset(db->slot_lock[1], 0, sizeof(uint8_t) * round_up(db->size * 2, 8));
    db->redirect_idx = 0;
    set_rehash(db);

    return OK;
}

int ks_memdb_add(void * in_db,
        const void * key,   const size_t sk,
        const void * value, const size_t sv,
        const KS_MEMDB_ADD_MODE mode) {
    // add to second hash table when we are rehashing
    KS_MEMDB * db = in_db;
    int      i    = 0;
    uint64_t hash = 0;
    KS_MEMDB_REC * rec = NULL;

    for (i = 0; !rec && i < 2 && db->hash[i]; i++) {
        // lock slot [=[
        hash = HASH(key, sk) % (db->size << i);
        wait_and_lock_slot(db, i, hash);
        // find the record
        for (rec = db->hash[i][hash];
             rec && keycmp(rec->key, rec->sk, key, sk);
             rec = rec->next);
        unlock_slot(db, i, hash);
    }

    if (rec) {
        // key in hashtable
        switch (mode? mode: (db->mode? db->mode: ADDMODE_REPLACE)) {
            case ADDMODE_SKIP: return ADD_SKIPPED;

            case ADDMODE_REPLACE: {
                wait_and_lock_record(rec);
                free(rec->value);
                rec->value = memndup(value, sv);
                rec->sv    = sv;
                unlock_record(rec);
                break;
            }

            case ADDMODE_APPEND: {
                if (((char *)rec->value)[rec->sv]
                 || ((char *)value)[sv]) return APPEND_NONSTR;

                wait_and_lock_record(rec);
                void * temp = malloc(sv + rec->sv - 1);
                memcpy(temp, rec->value, rec->sv);
                memcpy(temp + rec->sv - 1, value, sv);
                free(rec->value);
                rec->value = temp;
                rec->sv   += sv;
                unlock_record(rec);
                break;
            }

            case ADDMODE_INCR: {
                if (rec->sv != 4) return INCR_NAN;
                wait_and_lock_record(rec);
                *(int32_t*)(rec->value)++;
                unlock_record(rec);
            }

            case ADDMODE_NOOP:
                break;
        }
    } else {
        // key not in hashtable
        int table = rehashing(db)? 1: 0;
        KS_MEMDB_REC * new_rec = (KS_MEMDB_REC *)
            malloc(sizeof(KS_MEMDB_REC));
        new_rec->key   = memndup(key,   sk);
        new_rec->value = memndup(value, sv);
        new_rec->sk    = sk;
        new_rec->sv    = sv;
        new_rec->next  = db->hash[table][hash];
        new_rec->flags = 0;
        wait_and_lock_slot(db, table, hash);
        db->hash[table][hash] = new_rec;
        unlock_slot(db, table, hash);
        db->used[table]++;
    }

    ks_memdb_need_rehash(db);
    ks_memdb_rehash_step(db);
    return OK;
}

int ks_memdb_delete(void * in_db, const void * key, const size_t sk) {
    KS_MEMDB * db = in_db;
    uint64_t hash = HASH(key, sk);
    KS_MEMDB_REC * rec = NULL;
    int i = 0;

    for (i = 0; !rec && db->hash[i] && i < 2; i ++) {
        uint64_t idx = hash % (db->size << i);
        KS_MEMDB_REC * rec_prev = NULL;
        // lock slot
        wait_and_lock_slot(db, i, idx);
        for (rec = db->hash[i][idx]; rec; rec = rec->next) {
            if (0 == keycmp(key, sk, rec->key, rec->sk)) {
                // found it
                if (rec_prev) {
                    rec_prev->next = rec->next;
                } else {
                    db->hash[i][idx] = NULL;
                }
                break;
            }
            rec_prev = rec;
        }
        unlock_slot(db, i, idx);
    }

    if (rec) {
        free(rec->key);
        free(rec->value);
        free(rec);
        return OK;
    } else {
        return NOT_FOUND;
    }
}

void ks_memdb_dumpdb(const void * in_db) {
#define space(n) do { \
    int idx = (n); \
    while(idx--) printf(" "); \
} while(0)
    const KS_MEMDB * db = in_db;
    int i = 0, pos = 0, lv = 0;
    KS_MEMDB_REC * rec = NULL;

    for (i = 0; i < 2; i++) {
        if (!db->hash[i]) break;
        printf("\n=== TABLE %d ===\n", i);
        for (pos = 0; pos < db->size << i; pos ++) {
            printf("Hash slot %d\n", pos);
            for (lv = 1, rec = db->hash[i][pos]; rec; rec = rec->next, lv++) {
                printf("  | key %s, value %s\n", (char*)rec->key, (char*)rec->value);
            }
        }
    }

    return;
#undef space
}

void ks_memdb_destory(void * in_db) {
    KS_MEMDB * db = in_db;
    KS_MEMDB_REC * rec = NULL;
    int i = 0, j = 0;

    for (i = 0; db->hash[i] && i < 2; i++) {
        for (j = 0; j < db->size << i; j++) {
            for (rec = db->hash[i][j]; rec; rec = rec->next) {
                free(rec->key);
                free(rec->value);
                free(rec);
            }
        }
    }

    free(db->slot_lock[0]);
    free(db->slot_lock[1]);
    free(db->hash[0]);
    free(db->hash[0]);

    memset(db, 0, sizeof(KS_MEMDB));
    return;
}

int ks_memdb_get_value(void * db,
        const char * key, const size_t sk,
        char * out_value, size_t * out_sv) {
    KS_MEMDB_REC * rec = ks_memdb_lookup(db, key, sk);
    if (rec) {
        *out_sv = rec->sv;
        memcpy(out_value, rec->value, rec->sv);
        return OK;
    } else {
        return NOT_FOUND;
    }
}
``
// vim: foldmethod=syntax ts=4
