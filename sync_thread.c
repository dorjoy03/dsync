/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "sync_data_mpmc_queue.h"
#include "sync_file.h"
#include "sync_thread.h"

/*
 * Dequeues sync_data entries from the queue and calls sync_file to do the
 * syncing. When threads are created for sync/copy work, this is the function
 * they will be running.
 *
 * Retruns NULL.
 */
void *
sync_thread_func(void *data)
{
	struct sync_thread_data *thread_data = data;

	while(true) {
		struct sync_data *sd;
		int ret = sync_data_mpmc_queue_dequeue(thread_data->Q, &sd);
		if (ret == 0) {
			int status = sync_file(sd->src, sd->dst, thread_data->force_copy);
			if (status == -1) {
				fprintf(stderr, "Failed to sync %s to %s\n", sd->src, sd->dst);
			}
			free(sd->dst);
			free(sd->src);
			free(sd);
		} else {
			int traverse_done = __atomic_load_n(&thread_data->traverse_done,
			                                    __ATOMIC_ACQUIRE);
			if (traverse_done == 1)
				break;
		}
	}

	return NULL;
}
