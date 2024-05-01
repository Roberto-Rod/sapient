#ifndef DEBUG_LOG_HPP
#define DEBUG_LOG_HPP
#include <syslog.h>
void log(int pri, const char * fmt, ...);
#endif
