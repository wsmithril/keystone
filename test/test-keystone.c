/*
 * unit test for libkeystone.so
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "keystone.h"

void unit_test(KS_DB * db) {
    char k[256], v[256];
    int i = 0;
    const int ntest = 5000;
    int ret = 0;

    // test - set/getstr
    printf("Testing for add string.\n");
    for (i = 0; i < ntest; i++) {
        sprintf(k, "%05d", i);
        ks_db_set_str(db, k, k);
        ks_db_get_str(db, k, v);
        if (strcmp(k, v)) {
            printf("get/set str fail. got %s, supposed %s\n", v, k);
            exit(1);
        }
    }

    // test - append
    printf("Testing for append string.\n");
    for (i = 0; i < ntest; i++) {
        sprintf(k, "%05d", i);
        ks_db_append(db, k, k);
        ks_db_get_str(db, k, v);
        if (strncmp(v, k, 5) || strcmp(v + 5, k)) {
            printf("append fail. got %s, except %s%s\n", v, k, k);
            exit(2);
        }
    }

    // test - set/get num
    printf("Testing for set/get number.\n");
    for (i = 0; i < ntest; i++) {
        int32_t num;
        sprintf(k, "num-%05d", i);
        if ((ret = ks_db_set_num(db, k, (int32_t)i))) {
            printf("Set key %s failed, ret = %d\n", k, ret);
            continue;
        }

        if ((ret = ks_db_get_num(db, k, &num))) {
            printf("Get %s fail, ret = %d\n", k, ret);
        }
        if (num != (int32_t)i) {
            printf("%s Get %d, supposed %d\n", k, num, (int32_t)i);
            exit(3);
        }
    }

    // testing incr/decr
    printf("testing for incr/decr\n");
    for (i = 0; i < ntest; i++) {
        int32_t num;
        sprintf(k, "num-%05d", i);
        if ((ret = i % 2? ks_db_incr(db, k): ks_db_decr(db, k))) {
            printf("incr/decr fail, key: %s, ret: %d\n", k, ret);
            continue;
        }

        if ((ret = ks_db_get_num(db, k, &num))) {
            printf("Get %s fail, ret = %d\n", k, ret);
            continue;
        }

        if (num != i + (i % 2? 1: -1)) {
            printf("incr/decr failed, got %d, supposed %d\n", num, i + (i % 2? 1: -1));
        }
    }

    printf("All done!\n");
    //(db->op.debug)(db->db);
}


int main(void) {
    KS_DB test_db;

    ks_db_create(&test_db, DBTYPE_MEMDB, 2);
    unit_test(&test_db);
    ks_db_drop(&test_db);

    return 0;
}
