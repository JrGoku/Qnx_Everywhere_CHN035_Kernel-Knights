#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wraps QNX's slogf(), so log entries survive even if this process
 * crashes (they live in slogger2, readable after the fact with
 * sloginfo). Call plog_init() once per process, then use plog_info/
 * warn/error/critical instead of printf. Also mirrors to stdout/stderr
 * so SSH output looks the same as before.
 */

void plog_init(const char *process_name);

void plog_info(const char *fmt, ...);
void plog_warn(const char *fmt, ...);
void plog_error(const char *fmt, ...);
void plog_critical(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
