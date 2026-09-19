#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/neutrino.h>

#include "rt.h"

void rt_configure_self(const char *thread_name, int priority, unsigned cpu_mask)
{
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = priority;

    int rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if (rc != 0) {
        fprintf(stderr,
            "%s: pthread_setschedparam(SCHED_FIFO, %d) failed: %s - "
            "continuing at default priority (likely missing PROCMGR_AID_PRIORITY ability "
            "for this user; run as root or grant the ability to get real SCHED_FIFO)\n",
            thread_name, priority, strerror(rc));
    } else {
        printf("%s: priority set to SCHED_FIFO %d\n", thread_name, priority);
    }

    if (ThreadCtl(_NTO_TCTL_RUNMASK, (void *)(uintptr_t)cpu_mask) == -1) {
        fprintf(stderr, "%s: ThreadCtl(_NTO_TCTL_RUNMASK, 0x%x) failed: %s\n",
                thread_name, cpu_mask, strerror(errno));
    } else {
        printf("%s: CPU affinity mask set to 0x%x\n", thread_name, cpu_mask);
    }
}
