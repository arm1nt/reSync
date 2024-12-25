#ifndef RESYNC_MEM_UTIL_H
#define RESYNC_MEM_UTIL_H

#include <stdlib.h>

static inline void
do_free(void **ptr)
{
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

#endif //RESYNC_MEM_UTIL_H
