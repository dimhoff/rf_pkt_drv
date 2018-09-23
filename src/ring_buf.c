/**
 * ring_buf.c - Byte based ring buffer
 *
 * Copyright (c) 2015, David Imhoff <dimhoff.devel@gmail.com>
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
#include "ring_buf.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void ring_buf_init(ring_buf_t *obj, size_t size)
{
	obj->buf = (uint8_t *) malloc(size);
	obj->size = size;
	obj->woff = obj->roff = 0;
}

void ring_buf_add(ring_buf_t *obj, const uint8_t *data, size_t len)
{
	if (len >= obj->size - 1) {
		// Special case where len >= capacity of buffer
		// In this case directly fill the whole buferr with the last
		// (size - 1) bytes
		data += len - (obj->size - 1);
		len = (obj->size - 1);

		memcpy(&obj->buf[0], data, len);
		obj->woff = len;
		obj->roff = 0;
	} else {
		// Make sure there is enough space
		const size_t available = ring_buf_bytes_free(obj);
		if (len > available) {
			ring_buf_consume(obj, len - available);
		}

		while (len) {
			const size_t writable = ring_buf_bytes_writable(obj);
			const size_t copy_len = (len <= writable) ? len : writable;

			memcpy(&obj->buf[obj->woff], data, copy_len);

			obj->woff += copy_len;
			if (obj->woff == obj->size)
				obj->woff = 0;

			len -= copy_len;
			data += copy_len;
		}
	}
}

void ring_buf_consume(ring_buf_t *obj, size_t cnt)
{
	assert(cnt <= ring_buf_bytes_used(obj));

	obj->roff = (obj->roff + cnt) % obj->size;

	if (obj->roff == obj->woff)
		obj->roff = obj->woff = 0;
}

void ring_buf_clear(ring_buf_t *obj)
{
	obj->roff = obj->woff = 0;
}

size_t ring_buf_size(ring_buf_t *obj)
{
	return obj->size;
}

size_t ring_buf_bytes_free(const ring_buf_t *obj)
{
	return obj->size - ring_buf_bytes_used(obj) - 1;
}

size_t ring_buf_bytes_used(const ring_buf_t *obj)
{
	if (obj->woff < obj->roff) {
		return obj->size + obj->woff - obj->roff;
	} else {
		return obj->woff - obj->roff;
	}
}

size_t ring_buf_bytes_readable(const ring_buf_t *obj)
{
	if (obj->woff < obj->roff) {
		return obj->size - obj->roff;
	} else {
		return obj->woff - obj->roff;
	}
}

size_t ring_buf_bytes_writable(const ring_buf_t *obj)
{
	//TODO: we don't want to export this function, since writing directly to the buffer is probably not a good idea? but does allow read() directly into buffer...
	if (obj->woff < obj->roff) {
		return obj->roff - obj->woff - 1;
	} else if (obj->roff == 0) {
		return obj->size - obj->woff - 1;
	} else {
		return obj->size - obj->woff;
	}
}

bool ring_buf_empty(const ring_buf_t *obj)
{
	return (obj->woff == obj->roff);
}

uint8_t *ring_buf_begin(ring_buf_t *obj)
{
	return &obj->buf[obj->roff];
}

void ring_buf_destroy(ring_buf_t *obj)
{
	free(obj->buf);
	obj->buf = NULL;
}
