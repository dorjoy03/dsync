# Copyright (c) 2024 Dorjoy Chowdhury
# SPDX-License-Identifier: BSD-2-Clause

CC := gcc
CFLAGS := -Wall -Wextra -Werror -std=c99 -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS := -lpthread

ifeq ($(DEBUG),1)
CFLAGS += -g -fno-omit-frame-pointer
endif

OS := $(shell uname)

SOURCES := \
src/copy_read_write.c \
src/copy_symlink.c \
src/dsync.c \
src/sync_data_mpmc_queue.c \
src/sync_directory.c \
src/sync_file.c \
src/sync_thread.c \
src/traverse.c \
src/utils.c

# Only Linux and FreeBSD support copy_file_range
ifeq ($(OS), Linux)
	SOURCES += src/copy_file_linux.c
else ifeq ($(OS), FreeBSD)
	SOURCES += src/copy_file_linux.c
else
	SOURCES += src/copy_file_portable.c
endif

HEADERS := \
src/copy_file.h \
src/copy_read_write.h \
src/copy_symlink.h \
src/mpmc_queue_generic.h \
src/sync_data_mpmc_queue.h \
src/sync_directory.h \
src/sync_file.h \
src/sync_thread.h \
src/traverse.h \
src/utils.h

OBJECTS := $(SOURCES:.c=.o)

dsync: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f dsync $(OBJECTS)

test: dsync
	tests/test.sh
