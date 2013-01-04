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
    const int ntest = 100;

    // test - set/getstr
    for (i = 0; i < ntest; i++) {
        uint32_t out;
        sprintf(k, "%05d", i);
        ks_db_set_str(db, k, k);
        ks_db_get_str(db, k, v);
        if (strcmp(k, v)) {
            printf("get/set str fail. got %s, supposed %s\n", v, k);
        }
    }


    printf("All done!");
}


int main(void) {
    KS_DB test_db;

    ks_db_create(&test_db, DBTYPE_MEMDB, 2);
    unit_test(&test_db);
    ks_db_drop(&test_db);

    return 0;
}
