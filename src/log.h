#ifndef __LOG_H__
#define __LOG_H__

#ifndef LOGLV
#define LOGLV 3
#endif

#include <stdlib.h>

int __log(int loglv,
          FILE * f,
          int line,
          const char * file,
          const char * fmt, ...);

#define debug(...) do { \
        __log(5, stdout, __LINE__, __FILE__, __VA_ARGS__); \
    } while (0)

#define log(...) do { \
        __log(4, stdout, __LINE__, __FILE__, __VA_ARGS__); \
    } while (0)

#define notice(...) do { \
        __log(3, stderr, __LINE__, __FILE__, __VA_ARGS__); \
    } while (0)

#define warnning(...) do { \
        __log(2, stderr, __LINE__, __FILE__, __VA_ARGS__); \
    } while (0)

#define assert(v) do { \
        if (v) break; \
        __log(1, stderr, __LINE__, __FILE__, "Assertion fail"); \
        exit(1); \
    } while (0)

#define error(...) do { \
        __log(0, stderr, __LINE__, __FILE__, __VA_ARGS__); \
    } while (0)

#endif

