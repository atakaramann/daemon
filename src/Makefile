# SPDX-License-Identifier: MIT
#
# Userspace build: the fwd daemon and its fwctl control client. Both link a
# shared ipc.o, so the wire protocol lives in exactly one place.

CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Werror -O2 -I../include

DAEMON  := fwd
CLI     := fwctl

# ipc.o is shared; the pattern rule below builds it once. kernel.o talks to
# the kernel module over /dev/fw and is only needed by the daemon.
DAEMON_OBJS := main.o \
               daemonize.o \
               logging.o \
               rules.o \
               handler.o \
               kernel.o \
               ipc.o

CLI_OBJS    := cli.o \
               ipc.o

.PHONY: all clean

all: $(DAEMON) $(CLI)

$(DAEMON): $(DAEMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(CLI): $(CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Every .o depends on every header (and the shared uapi header): the project
# is small, so rebuilding on any header change is cheaper than tracking exact
# dependencies by hand.
%.o: %.c $(wildcard *.h) $(wildcard ../include/*.h)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(DAEMON) $(CLI)
