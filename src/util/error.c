#include "error.h"

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

    char *msg = format_string_from_list(fmt, args);
    fprintf(stderr, "%s\n", msg);

    va_end(args);
    DO_FREE(msg);
    exit(EXIT_FAILURE);
}
