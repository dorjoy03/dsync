/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef SYNC_FILE_H
#define SYNC_FILE_H

#include <stdbool.h>

int sync_file(char *src, char *dst, bool force_copy);

#endif /* SYNC_FILE_H */
