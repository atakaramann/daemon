# SPDX-License-Identifier: MIT

# Compiler.
CC := gcc

# Build with GNU C11 and treat all warnings as errors.
CFLAGS := -std=gnu11 -Wall -Wextra -Werror -O2

# Source directory and output binary name.
SRCDIR := src
BIN := firewalld

# All source files under src/.
SRCS := $(wildcard $(SRCDIR)/*.c)

# Matching object files for each source.
OBJS := $(SRCS:.c=.o)

.PHONY: all clean

# Default build target.
all: $(BIN)

# Link all object files into the final executable.
$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile each source file into its corresponding object file.
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Remove all generated files.
clean:
	rm -f $(OBJS) $(BIN)