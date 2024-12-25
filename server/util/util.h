#ifndef RESYNC_UTIL_H
#define RESYNC_UTIL_H

#include <stdio.h>
#include <stdlib.h>

#include "mem_util.h"

#ifdef LOG
#define L(x) do { \
    fprintf(stdout, "%s\n", x); \
    } while (0)
#else
#define L(X) do {} while (0)
#endif

#ifdef DEBUG
#define D(x, y) do { \
    fprintf(stderr, "%s: %s\n", x, y); \
    } while (0)
#else
#define D(x,y) do {} while(0)
#endif

static void
fatal_error(const char *name)
{
    perror(name);
    exit(EXIT_FAILURE);
}

#endif //RESYNC_UTIL_H
