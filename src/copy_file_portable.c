/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include "copy_file.h"
#include "copy_read_write.h"
#include "utils.h"

/*
 * Copy regular file ${src} to ${dst} with ${mode}. This is the portable version
 * that should work in all the POSIX systems.
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

	/* We don't call posix_fadvise like the linux version as posix_fadvise
	   may not be available in all systems. */

	ret = copy_read_write(src_fd, dst_fd, size);
	if (ret == -1) {
		err = "Failed to copy %s to %s";
		print_error_and_reset_errno(errno, err, dst);
		goto err2;
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
