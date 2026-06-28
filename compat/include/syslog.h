#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

#define LOG_CONS    0x02
#define LOG_LOCAL4  0

static __inline void openlog(const char *ident, int option, int facility)
{
    (void)ident;
    (void)option;
    (void)facility;
}

static __inline void syslog(int priority, const char *format, ...)
{
    (void)priority;
    (void)format;
}

static __inline void closelog(void)
{
}

#ifdef __cplusplus
}
#endif
