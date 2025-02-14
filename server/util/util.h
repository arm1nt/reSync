#ifndef RESYNC_UTIL_H
#define RESYNC_UTIL_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "mem_util.h"
#include "string_util.h"

#define _GNU_SOURCE

static char *format_string(const char *, va_list);

#ifdef RESYNC_LOGGING
#define LOG(fmt, ...) log_msg(fmt, ##__VA_ARGS__)
#else
#define LOG(fmt, ...) do {} while(0)
#endif

static void
log_msg(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg = format_string(fmt, args);

    fprintf(stdout, "%s\n", msg);

    va_end(args);
    DO_FREE(msg);
}

static void
fatal_error(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

static void
fatal_custom_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *msg = format_string(fmt, args);

    fprintf(stderr, "%s\n", msg);

    va_end(args);
    DO_FREE(msg);

    exit(EXIT_FAILURE);
}

static char *
format_string(const char *fmt, va_list args)
{
    char *buffer = NULL;
    int result = vasprintf(&buffer, fmt, args);

    if (result == -1) {
        fatal_error("vasprintf");
    }

    return buffer;
}

#endif //RESYNC_UTIL_H
