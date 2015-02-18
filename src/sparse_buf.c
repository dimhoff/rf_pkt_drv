/**
 * sparse_buf.c - Buffer in which only parts of the address space is valid
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
#include "sparse_buf.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * Amount of bits in an unsigned int type
 */
#define UINT_BIT_CNT (sizeof(unsigned int) * 8)

/**
 * Mark offset as valid data
 */
static inline void sparse_buf_make_valid(sparse_buf_t *obj, size_t off)
{
	const size_t idx = off / UINT_BIT_CNT;
	const unsigned int mask = 1 << (off % UINT_BIT_CNT);

	if (off >= obj->size) {
		return;
	}

	obj->valid[idx] |= mask;
}

int sparse_buf_init(sparse_buf_t *obj, size_t size)
{
	assert(size < SPARSE_BUF_OFF_END);
	assert(size + UINT_BIT_CNT - 1 > size);

	obj->size = size;
	obj->values = (uint8_t *) malloc(size);
	obj->valid = (unsigned int *)
		     calloc((size + UINT_BIT_CNT - 1) / UINT_BIT_CNT,
			    sizeof(unsigned int));

	if (obj->values == NULL || obj->valid == NULL) {
		free(obj->values);
		free(obj->valid);
		return -1;
	}

	return 0;
}

void sparse_buf_destroy(sparse_buf_t *obj)
{
	free(obj->values);
	free(obj->valid);
	memset(obj, 0, sizeof(sparse_buf_t));
}

void sparse_buf_clear(sparse_buf_t *obj)
{
	memset(obj->valid, 0,
	       ((obj->size + UINT_BIT_CNT - 1) / UINT_BIT_CNT) *
	       sizeof(unsigned int));
}

int sparse_buf_write(sparse_buf_t *obj, size_t off, uint8_t val)
{
	if (off >= obj->size) {
		return -1;
	}

	obj->values[off] = val;
	sparse_buf_make_valid(obj, off);

	return 0;
}

size_t sparse_buf_next_valid(const sparse_buf_t *obj, size_t off)
{
	while (off < obj->size) {
		if (sparse_buf_is_valid(obj, off))
			return off;
		off++;
	}

	return SPARSE_BUF_OFF_END;
}

size_t sparse_buf_next_invalid(const sparse_buf_t *obj, size_t off)
{
	while (off < obj->size) {
		if (! sparse_buf_is_valid(obj, off))
			return off;
		off++;
	}

	return SPARSE_BUF_OFF_END;
}

size_t sparse_buf_valid_length(const sparse_buf_t *obj, size_t off)
{
	int retval = 0;
	while (off < obj->size) {
		if (! sparse_buf_is_valid(obj, off))
			return retval;
		off++;
		retval++;
	}

	return retval;
}
