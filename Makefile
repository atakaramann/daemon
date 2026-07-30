# SPDX-License-Identifier: MIT
#
# Top-level coordinator for the fw firewall. Delegates to src/ (the fwd
# daemon and fwctl client) and kernel/ (the fw.ko netfilter module).

.PHONY: all daemon module clean check load unload reload

all: daemon module

# Userspace: fwd + fwctl.
daemon:
	$(MAKE) -C src

# Kernel module: fw.ko.
module:
	$(MAKE) -C kernel

clean:
	$(MAKE) -C src clean
	$(MAKE) -C kernel clean

# Kernel style check over both userspace and kernel sources, if checkpatch
# is present. It is a GPL tool fetched separately, so it stays gitignored.
CHECKPATCH  := ./checkpatch.pl
CHECK_FLAGS := --no-tree --terse --file

check:
	@if [ -f $(CHECKPATCH) ]; then \
		$(CHECKPATCH) $(CHECK_FLAGS) src/*.c src/*.h \
			kernel/*.c kernel/*.h include/*.h; \
	else \
		echo "checkpatch.pl not found in project root -- skipping"; \
		echo "get it from the Linux kernel: scripts/checkpatch.pl"; \
	fi

# Convenience wrappers for module load/unload during development (need root).
load:
	sudo insmod kernel/fw.ko

unload:
	sudo rmmod fw

reload: unload load
