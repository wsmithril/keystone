/*
 * storage common part
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

enum KS_ADD_MODE {
    ADDMODE_NOOP = 0,
    ADDMODE_SKIP,
    ADDMODE_APPEND,
    ADDMODE_REPLACE,
    ADDMODE_INCR,
    ADDMODE_DECR,
};
typedef enum KS_ADD_MODE KS_ADD_MODE;

// errors
#define OK      0
#define NOMEM   1
#define MEMFULL 2
#define EXTEND_ERR  3

#define ADD_SKIPPED   4
#define NOT_FOUND     5
#define APPEND_NONSTR 6
#define INCR_NAN      7

void * memndup(const void *, const size_t);
int keycmp(const void *, const size_t, const void *, const size_t);

#endif /* _STORAGE_H_ */
