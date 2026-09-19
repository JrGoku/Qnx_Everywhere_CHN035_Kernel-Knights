/*
 * Every call goes to slogf() (survives this process crashing, readable
 * later via sloginfo) and to stdout/stderr for live SSH viewing.
 * _SLOGC_TEST is the standard slog code for app-level ad-hoc logging.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/slog.h>
#include <sys/slogcodes.h>

#include "logger.h"

#define LOG_BUF_SZ 512
#define PLOG_SLOGCODE _SLOGC_TEST

static char g_process_name[64] = "unknown_proc";

static void vplog(int severity, FILE *mirror, const char *level_tag, const char *fmt, va_list ap)
{
    char msg[LOG_BUF_SZ];
    vsnprintf(msg, sizeof(msg), fmt, ap);

    slogf(PLOG_SLOGCODE, severity, "[%s] %s", g_process_name, msg);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(mirror, "[%ld.%03ld][%s][%s] %s\n",
            (long)ts.tv_sec, ts.tv_nsec / 1000000L, g_process_name, level_tag, msg);
    fflush(mirror);
}

void plog_init(const char *process_name)
{
    if (process_name && *process_name) {
        strncpy(g_process_name, process_name, sizeof(g_process_name) - 1);
        g_process_name[sizeof(g_process_name) - 1] = '\0';
    }
    plog_info("logger initialized (pid=%d) - full history recoverable later with `sloginfo`",
              (int)getpid());
}

void plog_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vplog(_SLOG_INFO, stdout, "INFO", fmt, ap);
    va_end(ap);
}

void plog_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vplog(_SLOG_WARNING, stdout, "WARN", fmt, ap);
    va_end(ap);
}

void plog_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vplog(_SLOG_ERROR, stderr, "ERROR", fmt, ap);
    va_end(ap);
}

void plog_critical(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vplog(_SLOG_CRITICAL, stderr, "CRITICAL", fmt, ap);
    va_end(ap);
}
