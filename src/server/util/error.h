#ifndef RESYNC_ERROR_H
#define RESYNC_ERROR_H

#include "string.h"
#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void fatal_error(const char *name);

void fatal_custom_error(const char *fmt, ...);

#endif //RESYNC_ERROR_H
