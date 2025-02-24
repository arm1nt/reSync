#ifndef RESYNC_STRING_H
#define RESYNC_STRING_H

#define _GNU_SOURCE

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "error.h"

/**
 *
 * @param str
 * @return
 */
char *resync_strdup(const char *str);

/**
 *
 * @param fmt
 * @param ...
 * @return
 */
char *format_string(const char *fmt, ...);

/**
 *
 * @param fmt
 * @param args
 * @return
 */
char *format_string_from_list(const char *fmt, va_list args);

/**
 *
 * @param str
 * @return
 */
bool is_blank(const char *str);


bool is_equal(const char *str1, const char *str2);

#endif //RESYNC_STRING_H
