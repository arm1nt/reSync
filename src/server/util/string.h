#ifndef RESYNC_STRING_H
#define RESYNC_STRING_H

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "error.h"

char *
resync_strdup(const char *str)
{
    if (str == NULL) {
        return NULL;
    }

    char *duped = strdup(str);
    if (duped == NULL) {
        fatal_error("strdup");
    }

    return duped;
}

char *
format_string(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char *buffer = NULL;
    if (vasprintf(&buffer, fmt, args) == -1) {
        fatal_error("vasprintf");
    }

    va_end(args);
    return buffer;
}

char *
format_string_from_list(const char *fmt, va_list args)
{
    char *buffer = NULL;
    if (vasprintf(&buffer, fmt, args) == -1) {
        fatal_error("vasprintf");
    }

    return buffer;
}

bool
is_blank(const char *str)
{
    if (str == NULL) {
        return false;
    }

    ssize_t index = 0;
    while (str[index] != '\0') {
        if (isspace(str[index]) == 0) {
            return false;
        }
        index++;
    }

    return true;
}

#endif //RESYNC_STRING_H
