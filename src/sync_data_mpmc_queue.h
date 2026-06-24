/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef SYNC_DATA_MPMC_QUEUE_H
#define SYNC_DATA_MPMC_QUEUE_H

#include <stddef.h>

#include "sync_thread.h"

/* Opaque type */
struct sync_data_mpmc_queue;

struct sync_data_mpmc_queue *sync_data_mpmc_queue_init(size_t queue_length);
void sync_data_mpmc_queue_free(struct sync_data_mpmc_queue *Q);
int sync_data_mpmc_queue_enqueue(struct sync_data_mpmc_queue *Q, struct sync_data *data);
int sync_data_mpmc_queue_dequeue(struct sync_data_mpmc_queue *Q, struct sync_data **data);

#endif /* SYNC_DATA_MPMC_QUEUE_H */
