#ifndef RT_H
#define RT_H

/* SCHED_FIFO priority + CPU affinity for the calling thread. Both are
 * best-effort - SCHED_FIFO needs PROCMGR_AID_PRIORITY, which qnxuser
 * may not have, so a failure just logs a warning and keeps running at
 * default scheduling instead of aborting. */
void rt_configure_self(const char *thread_name, int priority, unsigned cpu_mask);

#endif /* RT_H */
