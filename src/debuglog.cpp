#include "debuglog.hpp"
#include <cstdio>
#include <cstdarg>
#include <syslog.h>

void log(int pri, const char * fmt, ...)
{
    char str[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(str, fmt, args);
    va_end(args);
    syslog(pri, str);

#ifdef DEBUG_PRINTF
    if (pri == LOG_WARNING)
    {
        printf("warning: ");
    }
    else if (pri == LOG_INFO)
    {
        printf("info: ");
    }
    else if (pri == LOG_ERR)
    {
        printf("error: ");
    }
    printf("%s\n", str);
#endif
}
