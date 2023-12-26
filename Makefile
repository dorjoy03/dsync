# Copyright (c) 2024 Dorjoy Chowdhury
# SPDX-License-Identifier: BSD-2-Clause

CC := gcc
CFLAGS := -Wall -Wextra -Werror -std=c99 -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS := -lpthread

OS := $(shell uname)

SOURCES := \
copy_read_write.c \
copy_symlink.c \
dsync.c \
sync_data_mpmc_queue.c \
sync_directory.c \
sync_file.c \
sync_thread.c \
traverse.c \
utils.c

# Only Linux and FreeBSD support copy_file_range
ifeq ($(OS), Linux)
	SOURCES += copy_file_linux.c
else ifeq ($(OS), FreeBSD)
	SOURCES += copy_file_linux.c
else
	SOURCES += copy_file_portable.c
endif

HEADERS := \
copy_file.h \
copy_read_write.h \
copy_symlink.h \
mpmc_queue_generic.h \
sync_data_mpmc_queue.h \
sync_directory.h \
sync_file.h \
sync_thread.h \
traverse.h \
utils.h

OBJECTS := $(SOURCES:.c=.o)

dsync: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f dsync $(OBJECTS)
