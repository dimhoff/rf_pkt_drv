/**
 * si443x.c - Si443x interface functions
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <endian.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "si443x.h"

static int _si443x_spi_transfer(int fd, bool write, uint8_t addr, uint8_t *data, size_t len)
{
	struct spi_ioc_transfer	xfer[2];
	int err;

	assert((addr & 0x80) == 0);
	assert(
		((addr == 0x7f) && (len <= 64)) ||	// addr 0x7f gives access to read/write FIFO's
		((len < 0x7f) && (addr <= 0x7f - len))
	);

	memset(xfer, 0, sizeof(xfer));

	if (write) {
		addr |= 0x80;
	}

	// Send (rw // addr)
	xfer[0].tx_buf = (unsigned long) &addr;
	xfer[0].len = 1;

	// Read data
	if (write) {
		xfer[1].tx_buf = (unsigned long) data;
	} else {
		xfer[1].rx_buf = (unsigned long) data;
	}
	xfer[1].len = len;

	err = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
	if (err < 0) {
		perror("SPI_IOC_MESSAGE");
		return -1;
	}

	return 0;
}

int si443x_read_reg(si443x_dev_t *dev, uint8_t addr, uint8_t *data)
{
	return _si443x_spi_transfer(dev->fd, false, addr, data, 1);
}

int si443x_read_regs(si443x_dev_t *dev, uint8_t addr, uint8_t *data, size_t len)
{
	return _si443x_spi_transfer(dev->fd, false, addr, data, len);
}

int si443x_write_reg(si443x_dev_t *dev, uint8_t addr, uint8_t data)
{
	return _si443x_spi_transfer(dev->fd, true, addr, &data, 1);
}

int si443x_write_regs(si443x_dev_t *dev, uint8_t addr, const uint8_t *data, size_t len)
{
	return _si443x_spi_transfer(dev->fd, true, addr, (uint8_t *) data, len);
}

int si443x_open(si443x_dev_t *dev, const char *filename)
{
	int fd;

	fd = open(filename, O_RDWR);
	if (fd == -1) {
		return -1;
	}

	dev->fd = fd;

	return 0;
}

void si443x_dump_status(si443x_dev_t *dev)
{
	uint8_t buf[2];

	si443x_read_regs(dev, INTERRUPT_STATUS_1, buf, 2);
	fprintf(stderr, "Interrupt/Device Status: %.2x %.2x", buf[0], buf[1]);

	si443x_read_reg(dev, DEVICE_STATUS, &buf[0]);
	fprintf(stderr, " %.2x\n", buf[0]);
}

int si443x_reset(si443x_dev_t *dev)
{
	int err = -1;
	uint8_t val;

	err = si443x_write_reg(dev, OPERATING_MODE_AND_FUNCTION_CONTROL_1,
			OPERATING_MODE_AND_FUNCTION_CONTROL_1_XTON |
			OPERATING_MODE_AND_FUNCTION_CONTROL_1_SWRES);
	if (err != 0) {
		return err;
	}

	do {
		err = si443x_read_reg(dev, INTERRUPT_STATUS_2, &val);
		if (err != 0) {
			return err;
		}
	} while ((val & INTERRUPT_STATUS_2_ICHIPRDY) == 0);

	return 0;
}

int si443x_reset_rx_fifo(si443x_dev_t *dev)
{
	uint8_t ctrl[2];
	int err;

	// Get current control values
	err = si443x_read_regs(dev, OPERATING_MODE_AND_FUNCTION_CONTROL_1,
				ctrl, 2);
	if (err != 0)
		return err;

	// Disable RX mode
	if (ctrl[0] & OPERATING_MODE_AND_FUNCTION_CONTROL_1_RXON) {
		err = si443x_write_reg(dev,
			OPERATING_MODE_AND_FUNCTION_CONTROL_1,
			ctrl[0] & ~OPERATING_MODE_AND_FUNCTION_CONTROL_1_RXON);
		if (err != 0)
			return err;
		//TODO: verify that disabling RX stops current packet reception
	}

	// Clear RX FIFO
	err = si443x_write_reg(dev,
		OPERATING_MODE_AND_FUNCTION_CONTROL_2,
		ctrl[1] | OPERATING_MODE_AND_FUNCTION_CONTROL_2_FFCLRRX);
	if (err != 0)
		return err;

	err = si443x_write_reg(dev,
		OPERATING_MODE_AND_FUNCTION_CONTROL_2,
		ctrl[1] & ~OPERATING_MODE_AND_FUNCTION_CONTROL_2_FFCLRRX);
	if (err != 0)
		return err;

	// Re-enable RX mode
	if (ctrl[0] & OPERATING_MODE_AND_FUNCTION_CONTROL_1_RXON) {
		err = si443x_write_reg(dev,
			OPERATING_MODE_AND_FUNCTION_CONTROL_1, ctrl[0]);
		if (err != 0)
			return err;
	}

	return 0;
}

int si443x_configure(si443x_dev_t *dev, sparse_buf_t *regs)
{
	int err = -1;
	size_t off = 0;

	while ((off = sparse_buf_next_valid(regs, off))
			!= SPARSE_BUF_OFF_END) {
		const size_t len = sparse_buf_valid_length(regs, off);
		const uint8_t *startp = sparse_buf_at(regs, off);

		if (startp == NULL) {
			return -1;
		}

		err = si443x_write_regs(dev, off, startp, len);
		if (err != 0) {
			return err;
		}

		off += len;
	}
	return 0;
}

void si443x_close(si443x_dev_t *dev)
{
	close(dev->fd);
}
