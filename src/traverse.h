/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef TRAVERSE_H
#define TRAVERSE_H

int traverse_and_queue(char *src_paths[], char *dst_path,
                       struct sync_data_mpmc_queue *Q);

#endif /* TRAVERSE_H */
