/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "copy_file.h"
#include "copy_symlink.h"
#include "sync_file.h"
#include "utils.h"

/*
 * Syncs ${src} file to ${dst} file. If ${dst} doesn't exist or ${dst}'s size and
 * modification time don't match with ${src}, ${src} is copied to ${dst}. ${dst}'s
 * mode and timestamps are set equal to the ${src} if not already. Only regular
 * files or symbolic links are supported for syncing.
 *
 * Returns 0 on success, -1 on failure.
 */
int
sync_file(char *src, char *dst, bool force_copy)
{
	int ret;
	char *err;

	struct stat src_statbuf;
	ret = fstatat(AT_FDCWD, src, &src_statbuf, AT_SYMLINK_NOFOLLOW);
	if (ret != 0) {
		err = "Skipping sync of file %s. Failed to stat";
		print_error_and_reset_errno(errno, err, src);
		goto err0;
	}

	if (src_statbuf.st_size < 0) {
		fprintf(stderr, "Skipping sync of file %s. Got negative file size\n", src);
		goto err0;
	}

	if (force_copy == false) {
		struct stat dst_statbuf;
		ret = fstatat(AT_FDCWD, dst, &dst_statbuf, AT_SYMLINK_NOFOLLOW);
		if (ret != 0) {
			if (errno != ENOENT) {
				err = "Skipping sync of file %s. Failed to stat destination %s";
				print_error_and_reset_errno(errno, err, src, dst);
				goto err0;
			}
		} else if (src_statbuf.st_size == dst_statbuf.st_size &&
		           src_statbuf.st_mtim.tv_sec == dst_statbuf.st_mtim.tv_sec &&
		           src_statbuf.st_mtim.tv_nsec == dst_statbuf.st_mtim.tv_nsec) {
			if (src_statbuf.st_mode != dst_statbuf.st_mode) {
				ret = fchmodat(AT_FDCWD, dst, src_statbuf.st_mode, AT_SYMLINK_NOFOLLOW);
				if (ret != 0) {
					err = "Failed to update permissions for file %s";
					print_error_and_reset_errno(errno, err, dst);
					goto err0;
				}
			}
			return 0;
		}
	}

	switch (src_statbuf.st_mode & S_IFMT) {
	case S_IFLNK:
		ret = copy_symlink(src, dst, src_statbuf.st_size);
		if (ret != 0)
			goto err0;
		break;

	case S_IFREG:
		uintmax_t src_size = (uintmax_t) src_statbuf.st_size;
		ret = copy_file(src, dst, src_size, src_statbuf.st_mode);
		if (ret != 0)
			goto err0;
		break;

	default:
		err = "Failed to sync %s. Source must be a regular file or symbolic link\n";
		fprintf(stderr, err, src);
		goto err0;
		break;
	}

	struct timespec times[2] = {
		{src_statbuf.st_atim.tv_sec, src_statbuf.st_atim.tv_nsec},
		{src_statbuf.st_mtim.tv_sec, src_statbuf.st_mtim.tv_nsec}
	};
	ret = utimensat(AT_FDCWD, dst, times, AT_SYMLINK_NOFOLLOW);
	if (ret != 0) {
		err = "Failed to update timestamp for %s";
		print_error_and_reset_errno(errno, err, dst);
		goto err0;
	}

	return 0;

 err0:
	return -1;
}
