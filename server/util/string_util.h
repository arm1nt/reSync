#ifndef RESYNC_STRING_UTIL_H
#define RESYNC_STRING_UTIL_H

#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

typedef struct StringList {
    char *val;
    struct StringList *next;
} StringList;

static char *
resync_strdup(const char *str) {

    if (str == NULL) {
        return NULL;
    }

    char *duped = strdup(str);
    if (duped == NULL) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }

    return duped;
}

static bool
is_blank_string(const char *str)
{
    if (str == NULL) {
        return false;
    }

    size_t index = 0;

    while (str[index] != '\0') {
        if (isspace(str[index]) == 0) {
            return false;
        }
        index++;
    }

    return true;
}

#endif //RESYNC_STRING_UTIL_H
