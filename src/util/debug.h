#ifndef RESYNC_DEBUG_H
#define RESYNC_DEBUG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "string.h"
#include "memory.h"

#ifdef RESYNC_LOGGING
#define LOG(fmt, ...) log_msg(stdout, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_msg(stderr, fmt, ##__VA_ARGS__)

void log_msg(FILE *stream, const char *fmt, ...);
#else
#define LOG(fmt, ...) do {} while(0)
#define LOG_ERROR(fmt, ...) do {} while (0)
#endif

#endif //RESYNC_DEBUG_H
