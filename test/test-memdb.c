#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "storage_mem.h"

static int ks_memdb_ut(KS_MEMDB * db) {
    int i = 0;
    char k[128]; // key byffer
    char v[128];

    // TEST adding 100 keys
    for (i = 0; i < 100; i++) {
        sprintf(k, "%05d", i);
        ks_memdb_add(db, k, 6, k, 6, ADDMODE_REPLACE);
        ks_memdb_get_value(db, k, 6, v, NULL);
        // key in rec MUST be the same with k, but not the same pointer
        if (!((0 == memcmp(v, k, 6)))) {
            printf("TEST ADD SKIP failed\n");
            exit(1);
        }
    }
    printf("=== ADD SKIP test pass ===\n");

    // TEST adding 100 keys
    for (i = 0; i < 100; i++) {
        sprintf(k, "%05d", i);
        ks_memdb_add(db, k, 6, k, 6, ADDMODE_APPEND);
        ks_memdb_get_value(db, k, 6, v, NULL);
        if (!( (strncmp(k, v + 5, 5) == 0)
            && (strcmp( k, v + 5) == 0))) {
            printf("TEST ADD APPEND failed\n");
            exit(2);
        }
    }
    printf("=== ADD APPEND pass ===\n");

    v[0] = 0;
    // testing delete
    for (i = 1; i < 100; i += 2) {
        int ret = 0;
        sprintf(k, "%05d", i);
        ks_memdb_delete(db, k, 6);
        ret = ks_memdb_get_value(db, k, 6, v, NULL);
        if (!((v[0] == 0)
           && (ret == 5))) {
            printf("TEST DELETE failed, v[0] = %c, ret = %d\n", v[0], ret);
            exit(3);
        }
    }
    printf("=== DELETE pass ===\n");
    printf("\n The whole database:\n");
    ks_memdb_dumpdb(db);
    return 0;
}

int main(void) {
    KS_MEMDB test_db;
    ks_memdb_new(&test_db, 1);
    ks_memdb_ut(&test_db);

    return 0;
}

