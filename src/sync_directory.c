/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>

#include "utils.h"

/*
 * Syncs ${src} directory to ${dst} directory. If ${dst} doesn't exist, it is
 * created with ${src}'s mode. If ${dst} exists, it's mode is set to ${src}'s
 * mode if not already.
 *
 * Returns 0 on success, -1 on fatal error which means that the caller should
 * not move forward with the directory, -2 on non fatal error.
 */
int
sync_directory(char *src, char *dst)
{
	int ret;

	struct stat src_statbuf;
	ret = fstatat(AT_FDCWD, src, &src_statbuf,  AT_SYMLINK_NOFOLLOW);
	if (ret != 0)
		goto fatal_err;

	struct stat dst_statbuf;
	ret = fstatat(AT_FDCWD, dst, &dst_statbuf, AT_SYMLINK_NOFOLLOW);
	if (ret != 0) {
		if (errno == ENOENT) {
		 	ret = mkdir(dst, src_statbuf.st_mode);
			if (ret != 0)
				goto fatal_err;
		} else
			goto fatal_err;
	} else if (src_statbuf.st_mode != dst_statbuf.st_mode) {
		ret = fchmodat(AT_FDCWD, dst, src_statbuf.st_mode, AT_SYMLINK_NOFOLLOW);
		if (ret != 0) {
			char *err = "Failed to update mode of directory %s";
			print_error_and_reset_errno(errno, err, dst);
			goto non_fatal_err;
		}
	}

	return 0;

 non_fatal_err:
	return -2;

 fatal_err:
	return -1;
}
