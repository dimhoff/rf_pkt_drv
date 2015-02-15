/**
 * sparse_buf.h - Buffer in which only parts of the address space is valid
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
#ifndef __SPARSE_BUF_H__
#define __SPARSE_BUF_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

//TODO: better name!!!!!!! sparse buf is confusing


#define SPARSE_BUF_OFF_END SIZE_MAX

/**
 */
typedef struct {
	size_t size;	/**< size of buffer in bytes */
	/**
	 * Bit mask indicating which registers are valid
	 *
	 * A one in this bit mask indicates that the register corresponding
	 * with the bit offset contains a valid value.  Bits are numbered
	 * starting at the LSB of the first element, upto the MSB of the last
	 * element(eg. valid[0] & 0x01 => offset 0, valid[0] & 0x02 => offset
	 * 1, etc.)
	 */
	unsigned int *valid;
	uint8_t *values;	/**< register values */
} sparse_buf_t;

/**
 * Initialize sparse buffer
 */
int sparse_buf_init(sparse_buf_t *obj, size_t size);

/**
 * Cleanup sparse buffer
 */
void sparse_buf_destroy(sparse_buf_t *obj);

/**
 * Invalidate all data
 */
void sparse_buf_clear(sparse_buf_t *obj);

/**
 * Write a byte into buffer
 */
int sparse_buf_write(sparse_buf_t *obj, size_t off, uint8_t val);

/**
 * Get pointer into buffer at offset
 *
 * @returns	A pointer into the buffer or NULL if offset is beyond size.
 */
static inline uint8_t *sparse_buf_at(sparse_buf_t *obj, size_t off);

/**
 * Get buffer size
 */
static inline size_t sparse_buf_size(const sparse_buf_t *obj);

/**
 * Check if offset contains valid data
 */
static inline bool sparse_buf_is_valid(const sparse_buf_t *obj, size_t off);

/**
 * Find next valid byte in buffer
 *
 * If byte at 'off' is valid return 'off' else return the offset of the next
 * valid byte or SPARSE_BUF_OFF_END if there is non before the end of the
 * buffer.
 */
size_t sparse_buf_next_valid(const sparse_buf_t *obj, size_t off);

/**
 * Find next invalid byte in buffer
 *
 * If byte at 'off' is invalid return 'off' else return the offset of the next
 * invalid byte or SPARSE_BUF_OFF_END if there is non before the end of the
 * buffer.
 */
size_t sparse_buf_next_invalid(const sparse_buf_t *obj, size_t off);

/**
 * Return amount of sequential valid bytes starting at 'off'
 */
size_t sparse_buf_valid_length(const sparse_buf_t *obj, size_t off);


/*************** Static function implementations ***********************/

/**
 * Amount of bits in an unsigned int type
 */
#define UINT_BIT_CNT (sizeof(unsigned int) * 8)

static inline uint8_t *sparse_buf_at(sparse_buf_t *obj, size_t off)
{
	if (off >= obj->size) {
		return NULL;
	}

	return &obj->values[off];
}

static inline size_t sparse_buf_size(const sparse_buf_t *obj)
{
	return obj->size;
}

static inline bool sparse_buf_is_valid(const sparse_buf_t *obj, size_t off)
{
	if (off < obj->size) {
		const size_t idx = off / UINT_BIT_CNT;
		const unsigned int mask = 1 << (off % UINT_BIT_CNT);
		return (obj->valid[idx] & mask) != 0;
	}
	return false;
}

#undef UINT_BIT_CNT

#endif // __SPARSE_BUF_H__
