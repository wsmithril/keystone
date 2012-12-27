#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "storage_mem.h"

static int ks_memdb_ut(KS_MEMDB * db) {
    int i = 0;
    char k[128]; // key byffer
    KS_MEMDB_REC * rec;

#define show(rec) do { \
    if (rec) printf("key: %s, value: %s\n", (char*)rec->key, (char*)rec->value); \
    else printf("Not found\n"); \
} while (0)

    // TEST adding 100 keys
    for (i = 0; i < 100; i++) {
        sprintf(k, "%05d", i);
        ks_memdb_add(db, k, 6, k, 6, ADDMODE_REPLACE);
        rec = ks_memdb_lookup(db, k, 6);
        // key in rec MUST be the same with k, but not the same pointer
        if (!( (strcmp(rec->key, k) == 0)
            && (k != rec->key)
            && (6 == rec->sk))) {
            printf("TEST add not pass\n");
            exit(1);
        }
    }
    printf("=== ADD test pass ===\n");
    ks_memdb_dumpdb(db);

    // TEST adding 100 keys
    for (i = 0; i < 100; i++) {
        sprintf(k, "%05d", i);
        ks_memdb_add(db, k, 6, k, 6, ADDMODE_APPEND);
        rec = ks_memdb_lookup(db, k, 6);
        if (!( (strcmp(rec->key, k) == 0)
            && (k != rec->key)
            && (6 == rec->sk)
            && (strncmp(k, rec->value, 5) == 0)
            && (strcmp(k, rec->value + 5) == 0))) {
            printf("TEST append not pass\n");
            exit(2);
        }
    }
    printf("=== ADD append pass ===\n");
    ks_memdb_dumpdb(db);

    // testing delete
    for (i = 1; i < 100; i += 2) {
        sprintf(k, "%05d", i);
        ks_memdb_delete(db, k, 6);
        rec = ks_memdb_lookup(db, k, 6);
        if (rec) {
            printf("TEST DELETE not pass\n");
            exit(3);
        }
    }
    printf("=== ADD append pass ===\n");
    ks_memdb_dumpdb(db);
    return 0;
}

int main(void) {
    KS_MEMDB test_db;
    ks_memdb_new(&test_db, 1);
    ks_memdb_ut(&test_db);

    return 0;
}

