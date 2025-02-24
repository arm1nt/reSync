#ifndef RESYNC_ERROR_H
#define RESYNC_ERROR_H

#include "string.h"
#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#define CLEAR_ERROR_MSG(x) do {     \
        if ((x) != NULL) {          \
            *(x) = NULL;            \
        }                           \
    } while (0)

#define SET_ERROR_MSG_RAW(x,y) do {     \
        if ((x) != NULL) {              \
            *(x) = (y);                 \
        }                               \
    } while (0)

#define SET_ERROR_MSG(x,y) SET_ERROR_MSG_RAW(x, resync_strdup(y))

#define SET_ERROR_MSG_IF_EMPTY_RAW(x,y) do { \
        if ((x) != NULL && *(x) == NULL) {   \
            *(x) = (y);                      \
        }                                    \
    } while(0)

#define SET_ERROR_MSG_IF_EMPTY(x,y) SET_ERROR_MSG_IF_EMPTY_RAW(x, resync_strdup(y))

void fatal_error(const char *name);

void fatal_custom_error(const char *fmt, ...);

#endif //RESYNC_ERROR_H
