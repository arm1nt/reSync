#include "memory.h"

void *
do_realloc(void *orig_ptr, const ssize_t mem_req)
{
    void *ptr = realloc(orig_ptr, mem_req);

    if (ptr == NULL) {
        abort();
    }

    return ptr;
}

void *
do_malloc(const ssize_t mem_req)
{
    void *ptr = malloc(mem_req);

    if (ptr == NULL) {
        abort();
    }

    return ptr;
}

void *
do_calloc(const int number, const ssize_t mem_req)
{
    void *ptr = calloc(number, mem_req);

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
