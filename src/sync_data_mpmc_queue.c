/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

 #include <string.h>

#include "mpmc_queue_generic.h"
#include "sync_data_mpmc_queue.h"
#include "sync_thread.h"

static inline __attribute__((always_inline)) void copy_sync_data(struct sync_data *src, struct sync_data *dst)
{
	dst->src_len = src->src_len;
	memcpy(dst->src, src->src, dst->src_len);
	dst->dst_len = src->dst_len;
	memcpy(dst->dst, src->dst, dst->dst_len);
}

MPMC_QUEUE_DECLARE(sync_data, struct sync_data, copy_sync_data)
