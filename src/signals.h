/* SPDX-License-Identifier: MIT */
#ifndef SIGNALS_H
#define SIGNALS_H

#include <stdbool.h>

/* Register signal handlers. */
int signals_init(void);

/* Return true if shutdown was requested. */
bool signals_shutdown_requested(void);

/* Return true if log reload was requested. */
bool signals_reload_requested(void);

/* Clear the pending reload request. */
void signals_clear_reload_request(void);

#endif /* SIGNALS_H */
