/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "copy_read_write.h"

/*
 * Copy source file descriptor ${src} to destination file descriptor ${dst}
 * using a read write loop. This should be used for systems where we can't
 * utilize better system specific apis for copying.
 *
 * Returns 0 on success, -1 on failure. Sets errno on failure.
 */
int
copy_read_write(int src, int dst, uintmax_t size)
{
	/* Buffer size of 128KiB is picked up from gnu coreutils/src/io_blksize.h */
	size_t buf_size = (uintmax_t) (128 * 1024) < (uintmax_t) SSIZE_MAX
		? (128 * 1024)
		: SSIZE_MAX;
	uint8_t *buf = malloc(buf_size);
	if (buf == NULL)
		goto err0;

	uintmax_t bytes_left = size;
	while (bytes_left > 0) {
		size_t len = bytes_left > buf_size ? buf_size : (size_t) bytes_left;
		ssize_t bytes_read = read(src, buf, len);
		if (bytes_read == -1)
			goto err1;

		uintmax_t copied = bytes_read;
		uint8_t *buf_ptr =  buf;
		while (bytes_read > 0) {
			ssize_t bytes_written = write(dst, buf_ptr, bytes_read);
			if (bytes_written == -1)
				goto err1;
			bytes_read -= bytes_written;
			buf_ptr += bytes_written;
		}

		bytes_left -= (uintmax_t) copied;
	}

	free(buf);
	return 0;

 err1:
	free(buf);
 err0:
	return -1;
}
