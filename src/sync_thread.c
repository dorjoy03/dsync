/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdbool.h>

#include "sync_data_mpmc_queue.h"
#include "sync_file.h"
#include "sync_thread.h"

/*
 * Dequeues sync_data entries from the queue and calls try_sync_file to do the
 * syncing. When threads are created for sync/copy work, this is the function
 * they will be running.
 *
 * Returns NULL.
 */
void *
sync_thread_func(void *data)
{
	struct sync_thread_data *thread_data = data;
	struct sync_data sd;

	while(true) {
		int ret = sync_data_mpmc_queue_dequeue(thread_data->Q, &sd);
		if (ret == 0) {
			sync_file(sd.src, sd.dst, thread_data->force_copy);
		} else {
			int traverse_done = __atomic_load_n(&thread_data->traverse_done,
			                                    __ATOMIC_ACQUIRE);
			if (traverse_done == 1) {
				/* Even after we get traverse_done to be 1, we need to look for
				   entries in the queue as the following turn of events could lead
				   to the queue not being empty (the events are in order of time):
				   1. producer thread gets descheduled
				   2. consumer threads drain the queue and get ret == -1
				   3. consumer threads get descheduled at the else block before
				   reading traverse_done above
				   4. producer thread gets scheduled and adds a bunch to the queue
				   and sets traverse_done to 1
				   5. consumer threads now get scheduled and enter the else block
				   reading traverse_done to be 1 and the queue is not empty
				*/
				while (true) {
					ret = sync_data_mpmc_queue_dequeue(thread_data->Q, &sd);
					if (ret == 0)
						sync_file(sd.src, sd.dst, thread_data->force_copy);
					else
						break;
				}
				break;
			}
		}
	}

	return NULL;
}
