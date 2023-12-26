/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sync_data_mpmc_queue.h"
#include "sync_directory.h"
#include "sync_thread.h"
#include "traverse.h"
#include "utils.h"

#define BUF_SIZE 1024

/*
 * Returns the path suffix that needs to be appended to the destination directory
 * path. For example, if the source path is "/home/user/some/thing" and level
 * is 1, that means "some/thing" needs to be appended to the directory.
 *
 * Returns the (char *) in ${src} that is the desired suffix.
 */
static inline char *
get_path_suffix_at_level(char *src, size_t src_len, int level)
{
	assert(level >= 0 && (uintmax_t) (level) < UINT_MAX);
	assert(src_len > 0);

	uintmax_t lv = level;
	size_t ind = src_len - 1;
	while (true) {
		if (src[ind] != '/' || ind == 0)
			break;
		--ind;
	}

	if (ind == 0)
		return src;

	uintmax_t cnt = 0;
	char *rc = &src[ind];
	bool was_previous_char_slash = false;
	while (cnt < lv + 1) {
		if (src[ind] == '/') {
			if (was_previous_char_slash == false) {
				++cnt;
				was_previous_char_slash = true;
			}
		} else {
			was_previous_char_slash = false;
			rc = &src[ind];
		}

		if (ind == 0)
			break;

		--ind;
	}

	return rc;
}

/*
 * Prepares ${sd} by assigning proper source and destination paths to ${sd->src}
 * and ${sd->dst} respectively.
 *
 * Returns 0 on success, -1 on failure. Sets errno on failure.
 */
static inline int
prepare_sync_data(char *src, size_t src_len, char *dst, size_t dst_len, int level,
                  struct sync_data *sd)
{
	/* Make sure ${src_len + 1} will not wrap around. */
	if (src_len > SIZE_MAX - 1) {
		errno = ENOMEM;
		goto err0;
	}

	char *tmp;

	size_t total_src_len = src_len + 1;
	size_t buf_size = total_src_len < BUF_SIZE ? BUF_SIZE : total_src_len;
	tmp = malloc(buf_size);
	if (tmp == NULL)
		goto err0;
	sd->src = tmp;
	memcpy(sd->src, src, src_len);
	sd->src[total_src_len - 1] = '\0';

	char *suffix = get_path_suffix_at_level(src, src_len, level);
	size_t suffix_len = strlen(suffix);

	/* Make sure ${dst_len + suffix_len + 2} will not wrap around. */
	if (dst_len > SIZE_MAX - suffix_len || dst_len + suffix_len > SIZE_MAX - 2) {
		errno = ENOMEM;
		goto err1;
	}

	size_t total_dst_len = dst_len + suffix_len + 2;
	buf_size = total_dst_len < BUF_SIZE ? BUF_SIZE : total_dst_len;
	tmp = malloc(buf_size);
	if (tmp == NULL)
		goto err1;
	sd->dst = tmp;
	memcpy(sd->dst, dst, dst_len);
	sd->dst[dst_len] = '/';
	memcpy(sd->dst + dst_len + 1, suffix, suffix_len);
	sd->dst[total_dst_len - 1] = '\0';

	return 0;

 err1:
	free(sd->src);

 err0:
	return -1;
}

/*
 * Skips the ${ftsent} directory entry that needs to be skipped from traversal.
 */
static inline void
try_skip_directory(FTS *fts, FTSENT *ftsent)
{
	if (fts_set(fts, ftsent, FTS_SKIP) != 0) {
		char *err = "Failed to skip directory %s";
		print_error_and_reset_errno(errno, err, ftsent->fts_path);
	}
	return;
}

/*
 * Traverses the ${src_paths} and syncs sources to ${dst_path}. This function
 * handles the work of syncing directories itself. Files are added to the queue
 * for syncing which will be picked up by the sync threads. This function uses
 * the "fts" apis for traversing sources. Although not standardized by POSIX,
 * fts apis seem to be implemented by linux and the traditional bsd systems.
 *
 * ${src_paths} and ${dst_path} must be canonicalized absolute paths.
 *
 * Returns 0 on success, -1 on any kind of failure during traversal.
 */
int
traverse_and_queue(char *src_paths[], char *dst_path, struct sync_data_mpmc_queue *Q)
{
	int rc = 0;
	int ret;
	char *err;

	errno = 0;
 	FTS *fts = fts_open(src_paths, FTS_NOCHDIR | FTS_NOSTAT | FTS_PHYSICAL, NULL);
	if (fts == NULL) {
		print_error_and_reset_errno(errno, "Failed to traverse all the sources");
		rc = -1;
		goto done;
	}

	size_t sizeof_sync_data = sizeof(struct sync_data);
	size_t dst_len = strlen(dst_path);

	FTSENT *ftsent = NULL;
	char *dst_dir_buf = NULL;
	size_t dst_dir_buf_len = 0;

	while ((ftsent = fts_read(fts)) != NULL) {
		errno = 0;
		switch (ftsent->fts_info) {
		case FTS_D:
			if (ftsent->fts_pathlen <= 0 || ftsent->fts_level < 0)
				break;

			char *suffix = get_path_suffix_at_level(ftsent->fts_path,
			                                        ftsent->fts_pathlen,
			                                        ftsent->fts_level);
			size_t suffix_len = strlen(suffix);
			/* For source path '/', we don't need to create '/' in destination. */
			if (ftsent->fts_level == 0 && suffix[0] == '/')
				break;

			/* Make sure ${dst_len + suffix_len + 2} will not wrap around. */
			if (dst_len > SIZE_MAX - suffix_len ||
			    dst_len + suffix_len > SIZE_MAX - 2) {
				rc = -1;
				errno = ENOMEM;
				err = "Skipping sync of directory %s";
				print_error_and_reset_errno(errno, err, ftsent->fts_path);
				try_skip_directory(fts, ftsent);
				break;
			}

			size_t total_len = dst_len + suffix_len + 2;
			if (dst_dir_buf_len < total_len) {
				size_t buf_size = total_len < BUF_SIZE ? BUF_SIZE : total_len;
				char *tmp = realloc(dst_dir_buf, buf_size);
				if (tmp == NULL) {
					rc = -1;
					err = "Skipping sync of directory %s";
					print_error_and_reset_errno(errno, err, ftsent->fts_path);
					try_skip_directory(fts, ftsent);
					break;
				}
				dst_dir_buf = tmp;
				dst_dir_buf_len = buf_size;
			}

			memcpy(dst_dir_buf, dst_path, dst_len);
			dst_dir_buf[dst_len] = '/';
			memcpy(dst_dir_buf + dst_len + 1, suffix, suffix_len);
			dst_dir_buf[total_len - 1] = '\0';

			ret = sync_directory(ftsent->fts_path, dst_dir_buf);
			if (ret == -1) {
				rc = -1;
				err = "Skipping sync of directory %s";
				print_error_and_reset_errno(errno, err, ftsent->fts_path);
				try_skip_directory(fts, ftsent);
			}
			break;

		case FTS_F:
		case FTS_SL:
		case FTS_SLNONE:
		case FTS_NSOK:
			if (ftsent->fts_pathlen <= 0 || ftsent->fts_level < 0)
				break;

			/* ${sd}, ${sd->src} and ${sd->dst} will be free-d by the thread
			   that dequeues this ${sd}. */
			struct sync_data *sd = malloc(sizeof_sync_data);
			if (sd == NULL) {
				rc = -1;
				err = "Skipping sync of file %s";
				print_error_and_reset_errno(errno, err, ftsent->fts_path);
				break;
			}
			sd->src = sd->dst = NULL;
			ret = prepare_sync_data(ftsent->fts_path, ftsent->fts_pathlen,
			                        dst_path, dst_len, ftsent->fts_level, sd);
			if (ret != 0) {
				rc = -1;
				free(sd);
				err = "Skipping sync of file %s";
				print_error_and_reset_errno(errno, err, ftsent->fts_path);
				break;
			}

			while ((ret = sync_data_mpmc_queue_enqueue(Q, sd)) != 0)
				;

			break;

		case FTS_DEFAULT:
			fprintf(stderr, "Skipping %s. Unknown file type\n", ftsent->fts_path);
			break;

		case FTS_DNR:
			rc = -1;
			err = "Skipping sync of directory %s. Directory cannot be read";
			print_error_and_reset_errno(ftsent->fts_errno, err, ftsent->fts_path);
			break;

		case FTS_DC:
			rc = -1;
			err = "Skipping sync of directory %s. Directory causes cycle\n";
			fprintf(stderr, err, ftsent->fts_path);
			break;

		case FTS_NS:
		case FTS_ERR:
			rc = -1;
			err = "Failure during traversing for %s";
			print_error_and_reset_errno(ftsent->fts_errno, err, ftsent->fts_path);
			break;

		case FTS_DP:
		case FTS_DOT:
		default:
			break;
		}
	}

	if (errno) {
		rc = -1;
		print_error_and_reset_errno(errno, "Failure during traversing sources");
	}

	/* Ignore return value from fts_close. */
	fts_close(fts);
	free(dst_dir_buf);

 done:
	return rc;
}
