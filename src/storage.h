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

#endif /* _STORAGE_H_ */
