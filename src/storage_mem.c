/*
 * in mem database
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "config.h"
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

#define wait_and_lock_slot(db, t, idx) do { \
    while (slot_locked(db, t, idx)); \
    lock_slot(db, t, idx); \
} while(0)
// ================== STITIC METHODS ========================
static KS_MEMDB_REC *  ks_memdb_lookup(KS_MEMDB * db,
        const char * key, const size_t sk);

static void ks_memdb_rehash_one(KS_MEMDB * db);
static void ks_memdb_move_to_table(KS_MEMDB * db, int table, KS_MEMDB_REC * rec);
static int ks_memdb_extend(KS_MEMDB * db);
static int ks_memdb_need_rehash(KS_MEMDB* db);

static void * memndup(const void * p, const size_t sz);

// ================ Now, to bussiness ================

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
    return (len2 - len1 == 0)? 0: memcmp(key1, key2, len1);
}

int ks_memdb_new(KS_MEMDB * db, DBConfig * cfg) {
    uint64_t init_size = (cfg->memdb.size != 0)?
        cfg->memdb.size:
        MEMDB_DEFAULT;

    assert(db && cfg);

    // round up the to power of 2
    round2_64(init_size);

    debug("Creating database of size %lu, orig size: %lu", init_size, cfg->memdb.size);
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

static KS_MEMDB_REC * ks_memdb_lookup(KS_MEMDB * db,const char * key, const size_t sk) {
    uint64_t hash = HASH(key, sk);
    KS_MEMDB_REC * hr = NULL;
    int i = 0;

    // try two hash table, 2 here is not a magic
    for (i = 0; i < 2; i++) {
        if (!db->hash[i]) break;
        for (hr = (db->hash[i])[hash % (db->size << i)]; hr; hr = hr->next) {
            if (keycmp(hr->key, hr->sk, key, sk) == 0) break;
        }
        if (i == 0 && rehashing(db)) ks_memdb_rehash_one(db);
        if (hr) break;
    }

    return hr;
}

static void ks_memdb_rehash_one(KS_MEMDB * db) {
    KS_MEMDB_REC * hr = NULL;

    // rehash records in one hash slots
    if (db->used == 0) {
        // no more keys in first hash table, free this one
        free(db->hash[0]);
        db->hash[0] = db->hash[1];
        db->hash[1] = NULL;
        unset_rehash(db);
        return;
    }

    // get next slot in first hash table
    while ((db->hash[0])[db->redirect_idx]) {
        db->redirect_idx ++;
        if (db->redirect_idx >= db->size) {
            if (!db->used) return; // avoid possible deadlock
            db->redirect_idx = 0;
        }
    }

    debug("Rehashing record %ld", db->redirect_idx);;

    // lock hash slot in firest table [=[
    wait_and_lock_slot(db, 0, db->redirect_idx);
    for (hr = (db->hash[0])[db->redirect_idx]; hr; hr = hr->next) {
        ks_memdb_move_to_table(db, 1, hr);
        db->used[0]--;
    }
    // ]=]
    (db->hash[0])[db->redirect_idx] = NULL;
    unlock_slot(db, 0, db->redirect_idx);
    return;
}

static void ks_memdb_move_to_table(KS_MEMDB * db, int table, KS_MEMDB_REC * rec) {
    uint64_t hash = HASH(rec->key, rec->sk) % (db->size << table);
    // lock hash slot in second table [=[
    wait_and_lock_slot(db, table, hash);
    rec->next = (db->hash[table])[hash];
    (db->hash[table])[hash] = rec;
    db->used[table]++;
    // unlock ]=]
    unlock_slot(db, table, hash);
    return;
}

static int ks_memdb_need_rehash(KS_MEMDB* db) {
    if (rehashing(db)) return OK; // already rehashing
    if (db->used[0] * 1.0 / db->size > KS_MEMDB_THR_FILL) {
        if (ks_memdb_extend(db)) return EXTEND_ERR;
        db->redirect_idx = 0;
        set_rehash(db);
    }
    return OK;
}

static int ks_memdb_extend(KS_MEMDB * db) {
    if (db->hash[1]) return EXTEND_ERR;
    debug("extending hash table %x, orig size: %d", db, db->size);
    db->hash[1] = (KS_MEMDB_REC ** )
        malloc(sizeof(KS_MEMDB_REC *) * 2 * db->size);
    db->slot_lock[1] = (uint8_t *) malloc(sizeof(uint8_t) * (db->size + 3) / 4);
    if (!db->hash[1] || !db->slot_lock[1]) return NOMEM;
    memset(db->hash[1], 0, sizeof(KS_MEMDB_REC *) * 2 * db->size);
    memset(db->slot_lock[1], 0, sizeof(uint8_t) * (db->size + 3) / 4);
    db->redirect_idx = 0;
    set_rehash(db);
    return OK;
}

int ks_memdb_add(KS_MEMDB * db,
        const void * key,   const size_t sk,
        const void * value, const size_t sv,
        uint8_t mode) {
    // add to second hash table when we are rehashing
    int table = rehashing(db)? 1: 0;
    uint64_t hash = HASH(key, sk) % (db->size << table);
    KS_MEMDB_REC * rec = NULL;

    // lock slot [=[
    wait_and_lock_slot(db, table, hash);
    // find the record
    for (rec = db->hash[table][hash];
         rec && keycmp(rec->key, rec->sk, key, sk);
         rec = rec->next);

    if (rec) {
        // key in hashtable
        switch (mode? mode: (db->mode? db->mode: ADDMODE_REPLACE)) {
            case ADDMODE_SKIP: {
                unlock_slot(db, table, hash);
                return ADD_SKIPPED;
            }

            case ADDMODE_REPLACE: {
                free(rec->value);
                rec->value = memndup(value, sv);
                rec->sv    = sv;
                break;
            }

            case ADDMODE_APPEND: {
                void * temp = malloc(sv + rec->sv);
                memcpy(temp, rec->value, rec->sv);
                memcpy(temp + rec->sv, value, sv);
                free(rec->value);
                rec->value = temp;
                rec->sv   += sv;
                break;
            }

            case ADDMODE_INCR:
            case ADDMODE_NOOP:
                break;
        }
    } else {
        // key not in hashtable
        KS_MEMDB_REC * new_rec = (KS_MEMDB_REC *)
            malloc(sizeof(KS_MEMDB_REC));
        new_rec->key   = memndup(key,   sk);
        new_rec->value = memndup(value, sv);
        new_rec->sk    = sk;
        new_rec->sv    = sv;
        new_rec->next  = db->hash[table][hash];
        new_rec->flags = 0;
        db->hash[table][hash] = new_rec;
    }
    // unlock slot
    unlock_slot(db, table, hash);

    return OK;
}

int ks_memdb_delete(KS_MEMDB * db, const void * key, const size_t sk) {
    uint64_t hash =HASH(key, sk);
    KS_MEMDB_REC * rec = NULL;
    int i = 0;

    for (i = 0; i < 2; i ++) {
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
        if (rec) break;
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

#ifdef __MEMDB_TEST__
#define show(rec) do { \
    if (rec) printf("key: %s, value: %s\n", (char*)rec->key, (char*)rec->value); \
    else printf("Not found\n"); \
} while (0)

int main(void) {
    KS_MEMDB test_db;
    DBConfig cfg;
    KS_MEMDB_REC * rec;

    cfg.memdb.size = 1;

    ks_memdb_new(&test_db, &cfg);

    ks_memdb_add(&test_db, "abc", 4, "12345", 5, ADDMODE_REPLACE);
    rec = ks_memdb_lookup(&test_db, "abc", 4);
    show(rec);

    ks_memdb_add(&test_db, "abc1", 5, "12345", 5, ADDMODE_REPLACE);
    rec = ks_memdb_lookup(&test_db, "abc1", 5);
    show(rec);

    ks_memdb_add(&test_db, "abc", 4, "abcde", 5, ADDMODE_APPEND);
    rec = ks_memdb_lookup(&test_db, "abc", 4);
    show(rec);

    ks_memdb_delete(&test_db, "abc", 4);
    rec = ks_memdb_lookup(&test_db, "abc", 4);
    show(rec);

    rec = ks_memdb_lookup(&test_db, "abc1", 5);
    show(rec);

    return 0;
}

#endif


// vim: foldmethod=syntax ts=4
