#pragma once

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mstcpip.h>
#include <BaseTsd.h>
#include <bcrypt.h>
#include <conio.h>
#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <io.h>
#include <process.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifndef __attribute__
#define __attribute__(x)
#endif

#ifndef __func__
#define __func__ __FUNCTION__
#endif

#if !defined(__cplusplus) && !defined(inline)
#define inline __inline
#endif

typedef SSIZE_T ssize_t;
typedef int pid_t;

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN MAX_PATH
#endif
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR "\\"
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#define strdup _strdup
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define fileno _fileno
#define fstat _fstat
#define lstat _stat
#define stat _stat
#define open _open
#define read _read
#define write _write
#define close _close
#define dup _dup
#define chdir _chdir
#define umask _umask
#define getpid _getpid
#define access _access
#define strtok_r strtok_s

#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif

static __inline ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    size_t pos = 0;
    int ch;

    if (!lineptr || !n || !stream) {
        errno = EINVAL;
        return -1;
    }

    if (!*lineptr || *n == 0) {
        *n = 128;
        *lineptr = (char *)malloc(*n);
        if (!*lineptr) {
            *n = 0;
            errno = ENOMEM;
            return -1;
        }
    }

    while ((ch = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n * 2;
            char *new_line = (char *)realloc(*lineptr, new_size);
            if (!new_line) {
                errno = ENOMEM;
                return -1;
            }
            *lineptr = new_line;
            *n = new_size;
        }

        (*lineptr)[pos++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    if (pos == 0 && ch == EOF) {
        return -1;
    }

    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}

static __inline int ipmi_win32_set_tm_if_valid(struct tm *tm,
                                               int year,
                                               int month,
                                               int day,
                                               int hour,
                                               int minute,
                                               int second)
{
    if (year < 100) {
        year += (year >= 69) ? 1900 : 2000;
    }
    if (month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 60) {
        return 0;
    }

    tm->tm_year = year - 1900;
    tm->tm_mon = month - 1;
    tm->tm_mday = day;
    tm->tm_hour = hour;
    tm->tm_min = minute;
    tm->tm_sec = second;
    return 1;
}

static __inline char *strptime(const char *buf, const char *fmt, struct tm *tm)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int consumed = 0;

    if (!buf || !fmt || !tm) {
        return NULL;
    }

    if (strcmp(fmt, "%x %X") != 0) {
        return NULL;
    }

    if (sscanf_s(buf, "%d/%d/%d %d:%d:%d%n",
                 &month, &day, &year, &hour, &minute, &second, &consumed) == 6 &&
        ipmi_win32_set_tm_if_valid(tm, year, month, day, hour, minute, second)) {
        return (char *)buf + consumed;
    }

    if (sscanf_s(buf, "%d-%d-%d %d:%d:%d%n",
                 &year, &month, &day, &hour, &minute, &second, &consumed) == 6 &&
        ipmi_win32_set_tm_if_valid(tm, year, month, day, hour, minute, second)) {
        return (char *)buf + consumed;
    }

    if (sscanf_s(buf, "%d/%d/%d %d:%d:%d%n",
                 &year, &month, &day, &hour, &minute, &second, &consumed) == 6 &&
        ipmi_win32_set_tm_if_valid(tm, year, month, day, hour, minute, second)) {
        return (char *)buf + consumed;
    }

    return NULL;
}

static __inline char *index(const char *s, int c)
{
    return (char *)strchr(s, c);
}

static __inline char *rindex(const char *s, int c)
{
    return (char *)strrchr(s, c);
}

static __inline void usleep(unsigned int usec)
{
    Sleep((usec + 999u) / 1000u);
}

static __inline unsigned int sleep(unsigned int seconds)
{
    Sleep(seconds * 1000u);
    return 0;
}

static __inline struct tm *gmtime_r(const time_t *timer, struct tm *result)
{
    return gmtime_s(result, timer) == 0 ? result : NULL;
}

static __inline struct tm *localtime_r(const time_t *timer, struct tm *result)
{
    return localtime_s(result, timer) == 0 ? result : NULL;
}

static __inline int gettimeofday(struct timeval *tv, void *tz)
{
    FILETIME ft;
    unsigned __int64 now;
    (void)tz;
    if (!tv) {
        return -1;
    }
    GetSystemTimeAsFileTime(&ft);
    now = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    now -= 116444736000000000ULL;
    tv->tv_sec = (long)(now / 10000000ULL);
    tv->tv_usec = (long)((now % 10000000ULL) / 10ULL);
    return 0;
}

static __inline char *ipmi_win32_getpass(const char *prompt)
{
    static char password[256];
    size_t len = 0;
    int ch;

    if (prompt) {
        fputs(prompt, stderr);
        fflush(stderr);
    }

    memset(password, 0, sizeof(password));
    while ((ch = _getch()) != '\r' && ch != '\n') {
        if (ch == '\b') {
            if (len > 0) {
                len--;
            }
            continue;
        }
        if (ch == 3) {
            fputc('\n', stderr);
            errno = EINTR;
            return NULL;
        }
        if (isprint(ch) && len + 1 < sizeof(password)) {
            password[len++] = (char)ch;
        }
    }
    fputc('\n', stderr);
    password[len] = '\0';
    return password;
}

#define getpass(prompt) ipmi_win32_getpass(prompt)
#define getpassphrase(prompt) ipmi_win32_getpass(prompt)

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
int getopt(int argc, char * const argv[], const char *optstring);

typedef intptr_t ipmi_fd_t;
#define IPMI_INVALID_FD ((ipmi_fd_t)-1)

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

static __inline int ipmi_winsock_startup(void)
{
    static LONG initialized = 0;
    if (InterlockedCompareExchange(&initialized, 1, 0) == 0) {
        WSADATA wsa;
        int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (rc != 0) {
            InterlockedExchange(&initialized, 0);
            return rc;
        }
    }
    return 0;
}

static __inline int ipmi_socket_close(ipmi_fd_t fd)
{
    return closesocket((SOCKET)fd);
}

static __inline int ipmi_socket_disable_udp_connreset(ipmi_fd_t fd)
{
    BOOL new_behavior = FALSE;
    DWORD bytes_returned = 0;
    int rc = WSAIoctl((SOCKET)fd, SIO_UDP_CONNRESET,
                      &new_behavior, sizeof(new_behavior),
                      NULL, 0, &bytes_returned,
                      NULL, NULL);
    return rc == SOCKET_ERROR ? WSAGetLastError() : 0;
}

static __inline int ipmi_socket_send(ipmi_fd_t fd, const void *buf, int len, int flags)
{
    return send((SOCKET)fd, (const char *)buf, len, flags);
}

static __inline int ipmi_socket_recv(ipmi_fd_t fd, void *buf, int len, int flags)
{
    return recv((SOCKET)fd, (char *)buf, len, flags);
}

static __inline void ipmi_fd_set_socket(ipmi_fd_t fd, fd_set *set)
{
    FD_SET((SOCKET)fd, set);
}

static __inline int ipmi_fd_isset_socket(ipmi_fd_t fd, fd_set *set)
{
    return FD_ISSET((SOCKET)fd, set);
}

#define IPMI_FD_SET_SOCKET(fd, set) ipmi_fd_set_socket((fd), (set))
#define IPMI_FD_ISSET_SOCKET(fd, set) ipmi_fd_isset_socket((fd), (set))

#else
typedef int ipmi_fd_t;
#define IPMI_INVALID_FD (-1)
#define ipmi_socket_close close
#define ipmi_socket_send send
#define ipmi_socket_recv recv
#define ipmi_socket_disable_udp_connreset(fd) (0)
#define IPMI_FD_SET_SOCKET(fd, set) FD_SET((fd), (set))
#define IPMI_FD_ISSET_SOCKET(fd, set) FD_ISSET((fd), (set))
#endif
