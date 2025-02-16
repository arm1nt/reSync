#ifndef RESYNC_DEBUG_H
#define RESYNC_DEBUG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "string.h"
#include "memory.h"

#ifdef RESYNC_LOGGING
#define LOG(fmt, ...) log_msg(fmt, ##__VA_ARGS__)

void
log_msg(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char *msg = format_string_from_list(fmt, args);
    fprintf(stdout, "%s\n", msg);

    va_end(args);
    DO_FREE(msg);
}
#else
#define LOG(fmt, ...) do {} while(0)
#endif

#endif //RESYNC_DEBUG_H
