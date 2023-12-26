/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "copy_symlink.h"
#include "utils.h"

/*
 * Copy symbolic link ${src} itself (i.e., not what it points to) to ${dst}.
 * ${size} is symbolic link ${src}'s size.
 *
 * Returns 0 on success, -1 on failure.
 */
int
copy_symlink(char *src, char *dst, uintmax_t size)
{
	int ret;
	char *err;

	/* Make sure ${size + 1} can be represented by the return type of
	   readlinkat ssize_t. */
	if (size > (uintmax_t) (SSIZE_MAX - 1)) {
		errno = ENOMEM;
		err = "Skipping copy of symbolic link %s";
		print_error_and_reset_errno(errno, err, src);
		goto err0;
	}
	char *buf = malloc(size + 1);
	if (buf == NULL) {
		err = "Skipping copy of symbolic link %s";
		print_error_and_reset_errno(errno, err, src);
		goto err0;
	}
	ssize_t link_ret = readlinkat(AT_FDCWD, src, buf, size);
	if (link_ret == -1) {
		err = "Skipping copy of symbolic link %s. Failed to read contents";
		print_error_and_reset_errno(errno, err, src);
		goto err1;
	} else if ((uintmax_t) link_ret != size) {
		err = "Skipping copy of symbolic link %s. "
			"Stat size and read size did not match\n";
		fprintf(stderr, err, src);
		goto err1;
	}
	buf[link_ret] = '\0';
	ret = symlinkat(buf, AT_FDCWD, dst);
	if (ret != 0) {
		if (errno == EEXIST) {
			ret = unlinkat(AT_FDCWD, src, 0);
			if (ret != 0) {
				err = "Skipping copy of symbolic link %s. Failed to unlink "
					"existing symbolic link %s";
				print_error_and_reset_errno(errno, err, src, dst);
				goto err1;
			}
			ret = symlinkat(buf, AT_FDCWD, dst);
			if (ret != 0) {
				err = "Failed to create symbolic link %s";
				print_error_and_reset_errno(errno, err, dst);
				goto err1;
			}
		} else {
			err = "Failed to create symbolic link %s";
			print_error_and_reset_errno(errno, err, dst);
			goto err1;
		}
	}

	free(buf);
	return 0;

 err1:
	free(buf);
 err0:
	return -1;
}
