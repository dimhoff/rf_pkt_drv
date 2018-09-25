/**
 * si443x.c - Si443x interface functions
 *
 * Copyright (c) 2018, David Imhoff <dimhoff.devel@gmail.com>
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "si443x.h"
#include "si443x_enums.h"
#include "spi.h"
#include "error.h"

extern unsigned int verbose;

#define SI443X_FIFO_SIZE 64

static int _reset(rf_dev_t *dev);
static int _reset_rx_fifo(rf_dev_t *dev);
static int _sync_config(rf_dev_t *dev);
static int _configure(rf_dev_t *dev, sparse_buf_t *regs);
static void _dump_status(rf_dev_t *dev);

int rf_open(rf_dev_t *dev, const char *spi_path)
{
	int err = ERR_UNSPEC;
	int fd;
	uint8_t val;

	fd = open(spi_path, O_RDWR);
	if (fd == -1) {
		return ERR_SPI_OPEN_DEV;
	}

	dev->fd = fd;

	// Check device version
	TRY(spi_read_reg(dev->fd, DEVICE_TYPE, &val));
	if (val != DEVICE_TYPE_EZRADIOPRO) {
		err = ERR_RFM_CHIP_VERSION;
		goto fail;
	}

	// Read config
	TRY(_sync_config(dev));

	return ERR_OK;
fail:
	SAVE_ERRNO(rf_close(dev));
	return err;
}

void rf_close(rf_dev_t *dev)
{
	close(dev->fd);
	dev->fd = -1;
}

int rf_init(rf_dev_t *dev, sparse_buf_t *regs)
{
	int err = ERR_UNSPEC;

	// reset
	TRY(_reset(dev));

	// Program register configuration
	TRY(_configure(dev, regs));

	// enable receiver in multi packet FIFO mode
	//TODO: use defines
	TRY(spi_write_reg(dev->fd, OPERATING_MODE_AND_FUNCTION_CONTROL_1, 0x05));
	TRY(spi_write_reg(dev->fd, OPERATING_MODE_AND_FUNCTION_CONTROL_2, 0x10));

	err = ERR_OK;
fail:
	return err;
}

int rf_handle(rf_dev_t *dev, ring_buf_t *rx_buf, ring_buf_t *tx_buf)
{
	int err = ERR_UNSPEC;
	uint8_t buf[64];
	uint8_t hdrlen;
	uint8_t pktlen;
	uint8_t val;
	int i;

	// Check if packet available
	TRY(spi_read_reg(dev->fd, DEVICE_STATUS, &val));
	if ((val & DEVICE_STATUS_RXFFEM)) {
		return ERR_OK;
	}

	if (verbose) {
		_dump_status(dev);
	}

	// Wait till done receiving current packet
	//NOTE: DEVICE_STATUS.RXFFEM is also != 1 for partial packets!
	TRY(spi_read_reg(dev->fd, INTERRUPT_STATUS_2, &val));
	while ((val & INTERRUPT_STATUS_2_ISWDET)) {
		TRY(spi_read_reg(dev->fd, INTERRUPT_STATUS_2, &val));
		//TODO: add timeout?
	}

	// Read Header
	hdrlen = dev->txhdlen;
	if (dev->fixpklen == 0) {
		hdrlen += 1;
	}
	if (hdrlen) {
		TRY(spi_read_regs(dev->fd, FIFO_ACCESS, buf, hdrlen));
		if (verbose) {
			printf("Received header: \n");
			for (i = 0; i < hdrlen; i++) {
				printf("%.2x ", buf[i]);
			}
			putchar('\n');
			_dump_status(dev);
		}
	}
	if (dev->fixpklen == 0) {
		pktlen = buf[hdrlen - 1];
		if (pktlen > SI443X_FIFO_SIZE - 3) {
			fprintf(stderr, "ERROR: Packet len too big (%.2x)\n",
				pktlen);
			goto recover;
		}
	} else {
		pktlen = dev->fixpklen;
	}

	// Read Payload
	TRY(spi_read_regs(dev->fd, FIFO_ACCESS, &buf[hdrlen], pktlen));
	if (verbose) {
		printf("Received packet: \n");
		for (i = 0; i < pktlen; i++) {
			printf("%.2x ", buf[hdrlen + i]);
		}
		putchar('\n');

		_dump_status(dev);
	}

	// Check FIFO over/underflow condition
	TRY(spi_read_reg(dev->fd, DEVICE_STATUS, &val));
	if (val & (DEVICE_STATUS_FFOVFL |
			DEVICE_STATUS_FFUNFL)) {
		fprintf(stderr, "ERROR: Device "
			"overflow/underflow (%.2x)\n", val);
		goto recover;
	}

	// Add to ring buffer
	if (ring_buf_bytes_free(rx_buf) >= hdrlen + pktlen) {
		ring_buf_add(rx_buf, buf, hdrlen + pktlen);
	} else {
		fprintf(stderr, "Dropping packet, RX buffer overflow");
	}

	return ERR_OK;

recover:
	TRY(_reset_rx_fifo(dev));
	return ERR_OK;

fail:
	return err;
}

static int _reset(rf_dev_t *dev)
{
	int err = ERR_UNSPEC;
	uint8_t val;

	TRY(spi_write_reg(dev->fd, OPERATING_MODE_AND_FUNCTION_CONTROL_1,
			       OPERATING_MODE_AND_FUNCTION_CONTROL_1_XTON |
			       OPERATING_MODE_AND_FUNCTION_CONTROL_1_SWRES));

	do {
		//TODO: add timeout
		TRY(spi_read_reg(dev->fd, INTERRUPT_STATUS_2, &val));
	} while ((val & INTERRUPT_STATUS_2_ICHIPRDY) == 0);

	return ERR_OK;
fail:
	return err;
}

static int _reset_rx_fifo(rf_dev_t *dev)
{
	int err = ERR_UNSPEC;
	uint8_t ctrl[2];

	if (verbose) {
		printf("resetting RX fifo\n");
	}

	// Get current control values
	TRY(spi_read_regs(dev->fd, OPERATING_MODE_AND_FUNCTION_CONTROL_1,
			       ctrl, 2));

	// Disable RX mode
	if (ctrl[0] & OPERATING_MODE_AND_FUNCTION_CONTROL_1_RXON) {
		TRY(spi_write_reg(dev->fd,
				       OPERATING_MODE_AND_FUNCTION_CONTROL_1,
				       ctrl[0] & ~OPERATING_MODE_AND_FUNCTION_CONTROL_1_RXON));
		//TODO: verify that disabling RX stops current packet reception
	}

	// Clear RX FIFO
	TRY(spi_write_reg(dev->fd,
			       OPERATING_MODE_AND_FUNCTION_CONTROL_2,
			       ctrl[1] | OPERATING_MODE_AND_FUNCTION_CONTROL_2_FFCLRRX));

	TRY(spi_write_reg(dev->fd,
			       OPERATING_MODE_AND_FUNCTION_CONTROL_2,
			       ctrl[1] & ~OPERATING_MODE_AND_FUNCTION_CONTROL_2_FFCLRRX));

	// Re-enable RX mode
	if (ctrl[0] & OPERATING_MODE_AND_FUNCTION_CONTROL_1_RXON) {
		TRY(spi_write_reg(dev->fd,
			       OPERATING_MODE_AND_FUNCTION_CONTROL_1, ctrl[0]));
	}

	return ERR_OK;
fail:
	return err;
}

static int _sync_config(rf_dev_t *dev)
{
	int err = ERR_UNSPEC;
	uint8_t val;

	TRY(spi_read_reg(dev->fd, HEADER_CONTROL_2, &val));

	dev->txhdlen = (val >> HEADER_CONTROL_2_HDLEN_SHIFT) & HEADER_CONTROL_2_HDLEN_MASK;
	if ((val & HEADER_CONTROL_2_FIXPKLEN)) {
		TRY(spi_read_reg(dev->fd, TRANSMIT_PACKET_LENGTH, &dev->fixpklen));
	} else {
		dev->fixpklen = 0;
	}

	return ERR_OK;
fail:
	return err;
}

static int _configure(rf_dev_t *dev, sparse_buf_t *regs)
{
	int err = ERR_UNSPEC;
	size_t off = 0;

	while ((off = sparse_buf_next_valid(regs, off))
			!= SPARSE_BUF_OFF_END) {
		const size_t len = sparse_buf_valid_length(regs, off);
		const uint8_t *startp = sparse_buf_at(regs, off);

		if (startp == NULL) {
			return -1;
		}

		TRY(spi_write_regs(dev->fd, off, startp, len));

		off += len;
	}

	TRY(_sync_config(dev));

	return ERR_OK;
fail:
	return err;
}

static void _dump_status(rf_dev_t *dev)
{
	uint8_t buf[2];

	spi_read_regs(dev->fd, INTERRUPT_STATUS_1, buf, 2);
	fprintf(stderr, "Interrupt/Device Status: %.2x %.2x", buf[0], buf[1]);

	spi_read_reg(dev->fd, DEVICE_STATUS, &buf[0]);
	fprintf(stderr, " %.2x\n", buf[0]);
}
