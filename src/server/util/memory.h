#ifndef RESYNC_MEMORY_H
#define RESYNC_MEMORY_H

#include <stdlib.h>

#define DO_FREE(x) (do_free((void **) (&(x))))

void *
do_malloc(const ssize_t mem_req)
{
    void *ptr = malloc(mem_req);

    if (ptr == NULL) {
        abort();
    }

    return ptr;
}

void
do_free(void **ptr)
{
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

#endif //RESYNC_MEMORY_H
