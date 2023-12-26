/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

/*
 * Prints ${format} to stderr with a string describing the ${err} error code
 * and resets errno.
 */
void
print_error_and_reset_errno(int err, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	if (err != 0) {
		char err_buf[1024];
		strerror_r(err, err_buf, 1024);
		fprintf(stderr, " : %s", err_buf);
	}

	fprintf(stderr, "\n");
	errno = 0;

	return;
}
