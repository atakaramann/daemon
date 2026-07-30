# SPDX-License-Identifier: MIT
obj-m += fw.o
fw-objs := fw_mod.o fw_filter.o

# Shared uapi header lives one level up in include/.
ccflags-y += -I$(src)/../include

KDIR := /lib/modules/$(shell uname -r)/build

# CURDIR (not PWD) so 'make -C kernel' from the parent still points here.
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean