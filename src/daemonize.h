#ifndef DAEMONIZE_H             /* Prevent double inclusion */
#define DAEMONIZE_H

int become_daemon(void);        /* Returns 0 on success, -1 on error */

#endif