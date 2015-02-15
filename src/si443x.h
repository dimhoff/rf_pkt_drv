/**
 * si443x.h - Si443x interface functions
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
#ifndef __SI443X_H__
#define __SI443X_H__

#include "si443x_enums.h"
#include "sparse_buf.h"

#define SI443X_FIFO_SIZE 64

typedef struct {
	int fd;
} si443x_dev_t;

int si443x_open(si443x_dev_t *dev, const char *filename);
void si443x_close(si443x_dev_t *dev);

int si443x_reset(si443x_dev_t *dev);
int si443x_reset_rx_fifo(si443x_dev_t *dev);

int si443x_configure(si443x_dev_t *dev, sparse_buf_t *regs);

int si443x_read_reg(si443x_dev_t *dev, uint8_t addr, uint8_t *data);
int si443x_read_regs(si443x_dev_t *dev, uint8_t addr, uint8_t *data, size_t len);
int si443x_write_reg(si443x_dev_t *dev, uint8_t addr, uint8_t data);
int si443x_write_regs(si443x_dev_t *dev, uint8_t addr, const uint8_t *data, size_t len);

void si443x_dump_status(si443x_dev_t *dev);

#endif // __SI443X_H__
