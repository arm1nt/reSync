#ifndef RESYNC_ERROR_H
#define RESYNC_ERROR_H

#include "string.h"
#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#define SET_ERROR_MSG_RAW(x,y) do {     \
        if ((x) != NULL) {              \
            *(x) = (y);                 \
        }                               \
    } while (0)

#define SET_ERROR_MSG(x,y) SET_ERROR_MSG_RAW(x, resync_strdup(y))

#define SET_ERROR_MSG_WITH_CAUSE_RAW(error_msg, msg_ptr, cause_msg) do { \
        char *msg = msg_ptr;                                             \
        if ((error_msg) != NULL) {                                   \
            char *error_cause = ((cause_msg) != NULL) ? *(cause_msg) : NULL; \
            if (error_cause != NULL) {                               \
                *(error_msg) = format_string("%s: %s", msg, error_cause);\
            } else {                                                 \
                *(error_msg) = resync_strdup(msg);                                                     \
            }                                                        \
            DO_FREE(msg);                                            \
            DO_FREE(error_cause);                                    \
        }                                                            \
    } while (0)

#define SET_ERROR_MSG_WITH_CAUSE(error_msg, msg, cause_msg) do { \
        char *temp = resync_strdup(msg);                         \
        SET_ERROR_MSG_WITH_CAUSE_RAW(error_msg, temp, cause_msg);\
    } while (0)

void fatal_error(const char *name);

void fatal_custom_error(const char *fmt, ...);

#endif //RESYNC_ERROR_H
