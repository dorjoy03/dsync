/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define _DEFAULT_SOURCE /* for realpath */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "sync_data_mpmc_queue.h"
#include "sync_thread.h"
#include "traverse.h"
#include "utils.h"

#define QUEUE_SIZE 512
#define MAX_SYNC_THREAD_CNT 255

struct dsync_flags {
	bool force_copy;
	uint8_t sync_thread_cnt;
};

/*
 * Print usage to ${stream}.
 */
static void
usage(FILE *stream)
{
	char *usage =
		"Usage: dsync [OPTION]... SOURCE... DIRECTORY\n"
		"Sync/copy SOURCE(s) to DIRECTORY.\n\n"
		"  -f       force copy SOURCE(s) to DIRECTORY even if they are in sync\n"
		"  -j [N]   run N (max 255) threads that sync/copy source files\n\n"
		"By default (without the -f option), dsync will copy SOURCE(s) to DIRECTORY only\n"
		"if the files' size and modification time don't match (even if file in destination\n"
		"is newer than the corresponding source file). If SOURCE(s) themselves are symbolic\n"
		"links they will be resolved to their actual paths. dsync always preserves mode and\n"
		"timestamps. Multiple threads can be used to sync/copy using the -j option which\n"
		"can reduce total time in case of source directories with a lot of directories\n"
		"and a lot of small files in them. dsync always recursively syncs/copies all the\n"
		"contents of the given sources. Symbolic links inside SOURCE(s) are not followed\n"
		"but copied themselves. Extra directories or files in destination directory are\n"
		"not detected or deleted. dsync doesn't make sure data is written to disk.\n";
	fprintf(stream, usage);
	return;
}

int
main(int argc, char *argv[])
{
	int rc = 0;
	int ret;
	char *err;

	struct dsync_flags flags = {false, 1};
	int c;
	opterr = 0;
	while ((c = getopt(argc, argv, "fhj:")) != -1) {
		switch (c) {
		case 'f':
			flags.force_copy = true;
			break;
		case 'h':
			usage(stdout);
			rc = 0;
			goto done;
		case 'j':
			char *endptr = NULL;
			unsigned long value = strtoul(optarg, &endptr, 10);
			if (*endptr != '\0') {
				err = "Option -j should be provided with a value in range [1, %d].\n\n";
				fprintf(stderr, err, MAX_SYNC_THREAD_CNT);
				usage(stderr);
				goto err0;
			}
			if (value > 0 && value <= MAX_SYNC_THREAD_CNT) {
				flags.sync_thread_cnt = value;
			} else {
				err = "Number of threads must be in range [1, %d].\n\n";
				fprintf(stderr, err, MAX_SYNC_THREAD_CNT);
				usage(stderr);
				goto err0;
			}
			break;
		case '?':
			fprintf(stderr, "Unkown option -%c.\n\n", optopt);
			usage(stderr);
			goto err0;
		default:
			usage(stderr);
			goto err0;
		}
	}

	if (argc - optind < 2) {
		err = "At least one source and a destination directory must be provided.\n\n";
		fprintf(stderr, err);
		usage(stderr);
		goto err0;
	}

	struct stat dst_dir_statbuf;
	ret = fstatat(AT_FDCWD, argv[argc - 1], &dst_dir_statbuf, AT_SYMLINK_NOFOLLOW);
	if (ret != 0) {
		err = "Failed to stat destination directory %s";
		print_error_and_reset_errno(errno, err, argv[argc -1]);
		goto err0;
	}
	if (!S_ISDIR(dst_dir_statbuf.st_mode)) {
		fprintf(stderr, "%s is not a directory\n", argv[argc - 1]);
		goto err0;
	}

	char *dst_path = realpath(argv[argc - 1], NULL);
	if (dst_path == NULL) {
		err = "Failed to initialize absolute destination directory path";
		print_error_and_reset_errno(errno, err);
		goto err0;
	}

	int src_paths_len = argc - optind;
	char **src_paths = calloc(src_paths_len, sizeof(char *));
	if (src_paths == NULL) {
		err = "Failed to initialize source paths";
		print_error_and_reset_errno(errno, err);
		free(dst_path);
		goto err0;
	}

	for (int i = optind; i < argc - 1; ++i) {
		src_paths[i - optind] = realpath(argv[i], NULL);
		if (src_paths[i - optind] == NULL) {
			err = "Failed to initialize absolute source paths";
			print_error_and_reset_errno(errno, err);
			goto err1;
		}
	}

	struct sync_data_mpmc_queue *Q = sync_data_mpmc_queue_init(QUEUE_SIZE);
	if (Q == NULL) {
		print_error_and_reset_errno(errno, "Failed to initialize queue");
		goto err1;
	}

	struct sync_thread_data *thread_data = malloc(sizeof(struct sync_thread_data));
	if (thread_data == NULL) {
		print_error_and_reset_errno(errno, "Failed to allocate thread_data");
		goto err2;
	}

	thread_data->Q = Q;
	thread_data->force_copy = flags.force_copy;
	__atomic_store_n(&thread_data->traverse_done, 0, __ATOMIC_RELEASE);

	pthread_t threads[MAX_SYNC_THREAD_CNT];
	for (int i = 0; i < flags.sync_thread_cnt; ++i) {
		ret = pthread_create(&threads[i], NULL, sync_thread_func, thread_data);
		if (ret != 0) {
			print_error_and_reset_errno(ret, "Failed to create all threads");
			goto err3;
		}
	}

	ret = traverse_and_queue(src_paths, dst_path, Q);
	if (ret != 0)
		rc = 1;

	__atomic_store_n(&thread_data->traverse_done, 1, __ATOMIC_RELEASE);

	for (int i = 0; i < flags.sync_thread_cnt; ++i) {
		ret = pthread_join(threads[i], NULL);
		if (ret != 0) {
			rc = 1;
			err = "Failed to wait for thread %d to finish";
			print_error_and_reset_errno(ret, err, i);
		}
	}

	free(thread_data);
	sync_data_mpmc_queue_free(Q);
	for (int i = 0; i < src_paths_len; ++i)
		free(src_paths[i]);
	free(src_paths);
	free(dst_path);

 done:
	return rc;

 err3:
	free(thread_data);
 err2:
	sync_data_mpmc_queue_free(Q);
 err1:
	for (int i = 0; i < src_paths_len; ++i)
		free(src_paths[i]);
	free(src_paths);
	free(dst_path);
 err0:
	return 1;
}
