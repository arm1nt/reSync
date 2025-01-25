#ifndef RESYNC_MEM_UTIL_H
#define RESYNC_MEM_UTIL_H

#include <stdlib.h>

#define DO_FREE(x) (do_free((void **) &x))

static void *
do_malloc(const ssize_t mem_req)
{
    void *ptr = malloc(mem_req);

    if (!ptr) {
        abort();
    }

    return ptr;
}

static inline void
do_free(void **ptr)
{
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

#endif //RESYNC_MEM_UTIL_H
