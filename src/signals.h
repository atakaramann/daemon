/* SPDX-License-Identifier: MIT */
#ifndef SIGNALS_H
#define SIGNALS_H

#include <stdbool.h>

int signals_init(void);

bool signals_shutdown_requested(void);

bool signals_reload_requested(void);

void signals_clear_reload_request(void);

#endif /* SIGNALS_H */
