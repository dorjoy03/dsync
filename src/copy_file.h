/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef COPY_FILE_H
#define COPY_FILE_H

#include <sys/types.h>

#include <stdint.h>

/*
 * Copy ${src} to ${dst} with ${mode}.
 *
 * Currently, this is implemented by the linux specific copy_file_linux.c which
 * tries to use linux specific api and the portable copy_file_portable.c file.
 * If we want to utilize system specific better apis for copying, we need to
 * provide similar implementation of this api for other systems and update the
 * Makefile to use system specific implementation file during compilation.
 */
int copy_file(char *src, char *dst, uintmax_t size, mode_t mode);

#endif /* COPY_FILE_H */
