#ifndef RESYNC_MEMORY_H
#define RESYNC_MEMORY_H

#include <stdlib.h>

#define DO_FREE(x) (do_free((void **) (&(x))))

/**
 *
 * @param orig_ptr
 * @param mem_req
 * @return
 */
void *do_realloc(void *orig_ptr, const ssize_t mem_req);

/**
 *
 * @param mem_req
 * @return
 */
void *do_malloc(const ssize_t mem_req);

/**
 *
 * @param ptr
 */
void do_free(void **ptr);

#endif //RESYNC_MEMORY_H
