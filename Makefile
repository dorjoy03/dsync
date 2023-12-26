CC := gcc
CFLAGS := -Wall -Wextra -Werror -std=c99 -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS := -lpthread

SOURCES := \
sync_data_mpmc_queue.c \
utils.c \
copy_symlink.c \
copy_file_linux.c \
sync_file.c \
sync_thread.c \
sync_directory.c \
traverse.c \
dsync.c

HEADERS := \
mpmc_queue_generic.h \
sync_data_mpmc_queue.h \
utils.h \
copy_symlink.h \
copy_file.h \
sync_thread.h \
sync_file.h \
sync_directory.h \
traverse.h

OBJECTS := $(SOURCES:.c=.o)

dsync: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f dsync $(OBJECTS)
