/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef SYNC_THREAD_H
#define SYNC_THREAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PATH_SIZE 4096
#define CACHELINE_SIZE 64

struct sync_data {
	uint16_t src_len;
	char src[PATH_SIZE];
	uint16_t dst_len;
	char dst[PATH_SIZE];
};

/*
 * As sync_thread_data struct members are read by all the threads doing
 * sync/copy work, let's make sure they are on separate cachelines from
 * other potential malloc-ed memory to prevent false cacheline sharing.
 */
struct sync_thread_data {
	uint8_t pad0[CACHELINE_SIZE];
	struct sync_data_mpmc_queue *Q;
	int traverse_done;
	bool force_copy;
	uint8_t pad1[CACHELINE_SIZE];
};

void *sync_thread_func(void *data);

#endif /* SYNC_THREAD_H */
