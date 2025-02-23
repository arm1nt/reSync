#include "debug.h"

#ifdef RESYNC_LOGGING
void
log_msg(FILE* stream, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);


    char *msg = format_string_from_list(fmt, args);
    fprintf(stream, "%s\n", msg);

    va_end(args);
    DO_FREE(msg);
}
#endif
