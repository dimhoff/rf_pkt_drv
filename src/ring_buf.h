/**
 * ring_buf.h - Byte based ring buffer
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
#ifndef __RING_BUF_H__
#define __RING_BUF_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint8_t *buf;
	size_t size;
	size_t woff;
	size_t roff;
} ring_buf_t;

/**
 * Initialize ring buffer object
 *
 * Initialized the ring buffer, to be able to store size-1 bytes.
 *
 * @param obj	Object to initialize
 * @param size	Size of buffer
 */
void ring_buf_init(ring_buf_t *obj, size_t size);

/**
 * Destroy buffer object
 *
 * @param obj	Ring buffer object
 */
void ring_buf_destroy(ring_buf_t *obj);

/**
 * Copy data into buffer
 *
 * Copy data into ring buffer. When 'len' is bigger then the space available in
 * the buffer then old data is silently overwriten.
 *
 * @param obj	Ring buffer object
 * @param data	Data to copy into buffer
 * @param len	Length of data
 */
void ring_buf_add(ring_buf_t *obj, const uint8_t *data, size_t len);

/**
 * Move bytes out of buffer
 *
 * Copy bytes from ring buffer into provided buffer. And move the read pointer
 * forward.
 *
 * @param obj	Ring buffer object
 * @param data	Target buffer to copy bytes into
 * @param len	Amount of bytes to copy
 */
void ring_buf_get(ring_buf_t *obj, uint8_t *data, size_t len);

/**
 * Consume bytes from buffer
 *
 * Consume bytes by moving the read pointer 'cnt' bytes forward.
 *
 * @param obj	Ring buffer object
 * @param cnt	Amount of bytes to consume
 */
void ring_buf_consume(ring_buf_t *obj, size_t cnt);

/**
 * Clear all bytes from buffer
 *
 * Clears all bytes form the buffer and resets the read and write pointers.
 *
 * @param obj	Ring buffer object
 */
void ring_buf_clear(ring_buf_t *obj);

/**
 * Return begin of data
 *
 * Return raw pointer to the first byte of data in the buffer. Use
 * ring_buf_bytes_readable() to determine how many consecquitive bytes of data
 * can be read starting at the begin.
 *
 * @param obj	Ring buffer object
 *
 * @returns	Pointer to begin of data
 */
uint8_t *ring_buf_begin(ring_buf_t *obj);

size_t ring_buf_size(ring_buf_t *obj);
size_t ring_buf_bytes_free(const ring_buf_t *obj);
size_t ring_buf_bytes_used(const ring_buf_t *obj);
size_t ring_buf_bytes_readable(const ring_buf_t *obj);
size_t ring_buf_bytes_writable(const ring_buf_t *obj);
bool ring_buf_empty(const ring_buf_t *obj);
bool ring_buf_full(const ring_buf_t *obj);

#endif // __RING_BUF_H__
