/**
 * recursive_rmdir.c - Recursive remove of a directory
 *
 * Copyright (c) 2015, David Imhoff <dimhoff_devel@xs4all.nl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <ftw.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "recursive_rmdir.h"

/**
 * (Private) helper function for recursive_rmdir()
 *
 * @see recursive_rmdir()
 */
static int _recursive_rmdir_cb(const char *fpath, const struct stat *sb,
			       int typeflag, struct FTW *ftwbuf)
{
	int err = -1;

	if (typeflag == FTW_D || typeflag == FTW_DNR || typeflag == FTW_DP) {
		err = rmdir(fpath);
	} else {
		err = unlink(fpath);
	}

	if (err != 0) {
		fprintf(stderr, "Failed to remove '%s': %s\n",
			fpath, strerror(errno));
		return -2;
	}

	return 0;
}


/**
 * Max. number of file descriptors recursive_rmdir() may use.
 */
#define RECURSIVE_RMDIR_MAX_FD 32

int recursive_rmdir(const char *path)
{
	int err;

	err = nftw(path, _recursive_rmdir_cb, RECURSIVE_RMDIR_MAX_FD,
		   FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
	if (err != 0) {
		if (err != -2) {
			fprintf(stderr, "Unable to recursively remove "
				"'%s': %s\n",
				path, strerror(errno));
		}
		return -1;
	}

	return 0;
}
