#ifndef RESYNC_ERROR_H
#define RESYNC_ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "memory.h"

void
fatal_error(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

void
fatal_custom_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char *msg = NULL;
    if (vasprintf(&msg, fmt, args) == -1) {
        fatal_error("vasprintf");
    }
    fprintf(stderr, "%s\n", msg);

    va_end(args);
    DO_FREE(msg);
}

#endif //RESYNC_ERROR_H
