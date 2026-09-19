/*
 * name_attach()/name_open() are QNX's Global Name Space API - a kernel-
 * managed name -> channel directory any process on the node can query.
 * Despite being core Neutrino IPC, they're in <sys/dispatch.h>, not
 * <sys/neutrino.h> - caught the hard way at compile time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

#include "ipc.h"
#include "logger.h"

int ipc_server_attach(const char *name)
{
    name_attach_t *attach = name_attach(NULL, name, 0);
    if (attach == NULL) {
        plog_critical("ipc_server_attach: name_attach(\"%s\") failed: %s", name, strerror(errno));
        exit(1);
    }
    plog_info("ipc_server_attach: registered as \"%s\" (chid=%d)", name, attach->chid);
    return attach->chid;
}

int ipc_client_connect(const char *name)
{
    int coid;
    int attempt = 0;

    for (;;) {
        coid = name_open(name, 0);
        if (coid != -1) {
            break;
        }
        attempt++;
        /* log once immediately, then only every 20 tries - avoids spam */
        if (attempt == 1 || attempt % 20 == 0) {
            plog_warn("ipc_client_connect: waiting for \"%s\" to come up (attempt %d)...",
                      name, attempt);
        }
        usleep(100000); /* 100ms */
    }

    plog_info("ipc_client_connect: connected to \"%s\" (coid=%d)", name, coid);
    return coid;
}
