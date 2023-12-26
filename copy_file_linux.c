/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define _GNU_SOURCE /* for copy_file_range */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#include "copy_file.h"
#include "copy_read_write.h"
#include "utils.h"

/*
 * Copy regular file ${src} to ${dst} with ${mode}. This implementation uses
 * linux specific copy_file_range api for copying falling back to copy via
 * read write loop.
 *
 * Returns 0 on success, -1 on failure.
 */
int
copy_file(char *src, char *dst, uintmax_t size, mode_t mode)
{
	int ret;
	char *err;

	int src_fd = open(src, O_RDONLY);
	if (src_fd == -1) {
		print_error_and_reset_errno(errno, "Failed to open source %s", src);
		goto err0;
	}

	int dst_fd = open(dst, O_CREAT | O_TRUNC | O_WRONLY, mode);
	if (dst_fd == -1) {
		print_error_and_reset_errno(errno, "Failed to open destination %s", dst);
		goto err1;
	}

	posix_fadvise(src_fd, 0, 0, POSIX_FADV_SEQUENTIAL);

	errno = 0;
	uintmax_t bytes_left = size;
	while (bytes_left > 0) {
		size_t copy_len = bytes_left > (uintmax_t) SSIZE_MAX
			? SSIZE_MAX
			: (size_t) bytes_left;
		ssize_t copied = copy_file_range(src_fd, NULL, dst_fd, NULL, copy_len, 0);
		if (copied == -1)
			break;

		bytes_left -= (uintmax_t) copied;
	}

	if (errno) {
		/* If copy_file_range is not supported or cross-filesystem copy_file_range
		   is not supported, let's fallback to copying using read write loop. */
		if (bytes_left == size && (errno == EOPNOTSUPP || errno == EXDEV)) {
			ret = copy_read_write(src_fd, dst_fd, size);
			if (ret == -1) {
				err = "Failed to copy %s to %s";
				print_error_and_reset_errno(errno, err, src, dst);
				goto err2;
			}
		} else {
			err = "Failed to copy %s to %s";
			print_error_and_reset_errno(errno, err, src, dst);
			goto err2;
		}
	}

	ret = close(dst_fd);
	if (ret != 0) {
		err = "Failed to close file descriptor for destination %s";
		print_error_and_reset_errno(errno, err, dst);
		goto err1;
	}
	/* Ignore return value from close on src_fd as src is opened for reading only. */
	close(src_fd);
	/* Let's reset errno in case close failed. */
	errno = 0;
	return 0;

 err2:
	close(dst_fd);
 err1:
	close(src_fd);
 err0:
	errno = 0;
	return -1;
}
