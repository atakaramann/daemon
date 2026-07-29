# SPDX-License-Identifier: MIT
#
# Build for the fw firewall: the fwd daemon and its fwctl control client.
# Both link a shared ipc.o, so the wire protocol lives in exactly one place.

CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Werror -O2
SRCDIR  := src

DAEMON  := fwd
CLI     := fwctl

# Object files. ipc.o is shared, so it is listed for both binaries and the
# pattern rule below builds it once.
DAEMON_OBJS := $(SRCDIR)/main.o \
               $(SRCDIR)/daemonize.o \
               $(SRCDIR)/logging.o \
               $(SRCDIR)/rules.o \
               $(SRCDIR)/handler.o \
               $(SRCDIR)/ipc.o

CLI_OBJS    := $(SRCDIR)/cli.o \
               $(SRCDIR)/ipc.o

# checkpatch.pl is the Linux kernel style checker. It is not part of the
# source tree (it is a GPL tool fetched separately), so it stays gitignored.
CHECKPATCH  := ./checkpatch.pl
CHECK_FLAGS := --no-tree --terse --file

.PHONY: all clean check

all: $(DAEMON) $(CLI)

$(DAEMON): $(DAEMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(CLI): $(CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Every .o depends on every header: the project is small, so a rebuild on
# any header change is cheaper than tracking exact dependencies by hand.
$(SRCDIR)/%.o: $(SRCDIR)/%.c $(wildcard $(SRCDIR)/*.h)
	$(CC) $(CFLAGS) -c $< -o $@

# Run the kernel style checker over every source file, if it is present.
check:
	@if [ -f $(CHECKPATCH) ]; then \
		$(CHECKPATCH) $(CHECK_FLAGS) $(SRCDIR)/*.c $(SRCDIR)/*.h; \
	else \
		echo "checkpatch.pl not found in project root -- skipping"; \
		echo "get it from the Linux kernel: scripts/checkpatch.pl"; \
	fi

clean:
	rm -f $(SRCDIR)/*.o $(DAEMON) $(CLI)