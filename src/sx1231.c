/**
 * sx1231.c - sx1231 interface functions
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

#include "sx1231.h"
#include "sx1231_enums.h"
#include "spi.h"
#include "error.h"
#include "debug.h"

#define SX1231_FIFO_SIZE 66

static int _reset(rf_dev_t *dev);
static int _reset_rx_fifo(rf_dev_t *dev);
static int _sync_config(rf_dev_t *dev);
static int _configure(rf_dev_t *dev, sparse_buf_t *regs);
static int _switch_mode(rf_dev_t *dev, int mode);
static int _send_frame(rf_dev_t *dev, ring_buf_t *tx_buf);
static int _receive_frame(rf_dev_t *dev, ring_buf_t *rx_buf);
static void _dump_status(rf_dev_t *dev);
static void _dump_packet_status(rf_dev_t *dev);

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
	TRY(spi_read_reg(dev->fd, RegVersion, &val));
	if ((val & SX1231_VERSION_MASK) != SX1231_VERSION) {
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

	// Switch to receive mode
	TRY(_switch_mode(dev, OP_MODE_MODE_RX));

	err = ERR_OK;
fail:
	return err;
}

int rf_handle(rf_dev_t *dev, ring_buf_t *rx_buf, ring_buf_t *tx_buf)
{
	int err = ERR_UNSPEC;
	uint8_t irq_flags[2];

	TRY(spi_read_regs(dev->fd, RegIrqFlags1, irq_flags, 2));

	// Check FIFO over/underflow condition
	if (irq_flags[1] & IRQ_FLAGS2_FIFOOVERRUN) {
		fprintf(stderr, "ERROR: FIFO overrun\n");

		// clear flag & fifo
		TRY(spi_write_reg(dev->fd, RegIrqFlags2, IRQ_FLAGS2_FIFOOVERRUN));
	} else {
		while ((irq_flags[0] & IRQ_FLAGS1_SYNCADDRESSMATCH) &&
				!(irq_flags[1] & IRQ_FLAGS2_PAYLOADREADY)) {
			// Currently receiving packet, wait for completion or crc error
			TRY(spi_read_regs(dev->fd, RegIrqFlags1, irq_flags, 2));
			//TODO: add timeout...
		}

		if (irq_flags[1] & IRQ_FLAGS2_PAYLOADREADY) {
			TRY(_receive_frame(dev, rx_buf));
		}
	}

	if (! ring_buf_empty(tx_buf)) {
		TRY(_send_frame(dev, tx_buf));
	}

	err = 0;
fail:
	return err;
}

static int _reset(rf_dev_t *dev)
{
	/* TODO:
	int err = ERR_UNSPEC;
	uint8_t val;
	*/

	return ERR_OK;
}

static int _reset_rx_fifo(rf_dev_t *dev)
{
	int err = ERR_UNSPEC;

	TRY(_switch_mode(dev, OP_MODE_MODE_STDBY));
	TRY(_switch_mode(dev, OP_MODE_MODE_RX));

	return ERR_OK;
fail:
	return err;
}

static int _sync_config(rf_dev_t *dev)
{
	int err = ERR_UNSPEC;
	uint8_t val;

	TRY(spi_read_reg(dev->fd, RegPacketConfig1, &val));
	if (val & PACKET_CONFIG1_PACKETFORMAT) {
		dev->fixpklen = 0;
	} else {
		TRY(spi_read_reg(dev->fd, RegPayloadLength, &dev->fixpklen));
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

static int _switch_mode(rf_dev_t *dev, int mode)
{
	int err = ERR_UNSPEC;
	uint8_t val;

	assert((mode & ~0x1c) == 0);

	TRY(spi_write_reg(dev->fd, RegOpMode, mode));

	do {
		// TODO: add timeout
		TRY(spi_read_reg(dev->fd, RegIrqFlags1, &val));
	} while (! (val & IRQ_FLAGS1_MODEREADY));

	return ERR_OK;
fail:
	return err;
}

static int _send_frame(rf_dev_t *dev, ring_buf_t *tx_buf)
{
	int err = ERR_UNSPEC;
	uint8_t hdrlen;
	uint8_t pktlen;
	uint8_t val;

	// Determine next frame size
	hdrlen = 0;
	pktlen = dev->fixpklen;
	if (pktlen == 0) {
		pktlen = *ring_buf_begin(tx_buf);
		hdrlen = 1;
	}
	if (pktlen == 0 || pktlen > SX1231_FIFO_SIZE - 1) {
		err = ERR_RFM_TX_OUT_OF_SYNC;
		goto fail;
	}

	if (pktlen <= ring_buf_bytes_used(tx_buf)) {
		uint8_t pkt[SX1231_FIFO_SIZE];
		TRY(_switch_mode(dev, OP_MODE_MODE_STDBY));

		// Fill Fifo
		ring_buf_get(tx_buf, pkt, hdrlen + pktlen);
		TRY(spi_write_regs(dev->fd, RegFifo, pkt, pktlen));

		// Send frame
		TRY(_switch_mode(dev, OP_MODE_MODE_TX));
		do {
			TRY(spi_read_reg(dev->fd, RegIrqFlags2, &val));
		} while (! (val & IRQ_FLAGS2_PACKETSENT));

		TRY(_switch_mode(dev, OP_MODE_MODE_RX));
		// TODO: inter frame gap???
	}

	return ERR_OK;
fail:
	return err;
}

static int _receive_frame(rf_dev_t *dev, ring_buf_t *rx_buf)
{
	int err = ERR_UNSPEC;
	uint8_t buf[64];
	uint8_t hdrlen;
	uint8_t pktlen;
	bool drop;
	int i;

	DBG_PRINTF(DBG_LVL_LOW, "> Received packet: \n");
	DBG_EXEC(DBG_LVL_LOW, _dump_packet_status(dev));
	DBG_EXEC(DBG_LVL_HIGH, _dump_status(dev));

	// Read Header
	if (dev->fixpklen == 0) {
		TRY(spi_read_reg(dev->fd, RegFifo, &buf[0]));

		hdrlen = 1;
		pktlen = buf[0];

		if (pktlen == 0 || pktlen > SX1231_FIFO_SIZE - 1) {
			fprintf(stderr, "ERROR: Invalid Packet length(%u)\n",
				pktlen);
			TRY(_reset_rx_fifo(dev));
			err = ERR_OK;
			goto fail;
		}
	} else {
		hdrlen = 0;
		pktlen = dev->fixpklen;
	}

	// Read Payload
	TRY(spi_read_regs(dev->fd, RegFifo, &buf[hdrlen], pktlen));

	DBG_HEXDUMP(DBG_LVL_MID, &buf[hdrlen], pktlen);
	DBG_EXEC(DBG_LVL_HIGH, _dump_status(dev));

#if 1
	// Local CRC calculation
	// Currently calculates 0x8005 CRC-16 (IBM) on data only.
	// TODO: remove this and switch sensof to CRC-CCITT
	drop = false;
	if (true) {
		uint16_t crc = 0;
		const uint16_t CRC_16_POLY = 0x8005;
		if (pktlen < 2) {
			drop = true;
		} else {
			pktlen -= 2;
			for (i=hdrlen; i < pktlen; i++) {
				// from sensof:crc.c:crc16()
				uint8_t j=8;
				bool do_xor;
				uint8_t b = buf[i];

				while (j) {
					do_xor = (b ^ (crc >> 8)) & 0x80;

					crc = crc << 1;

					if (do_xor) {
						crc ^= CRC_16_POLY;
					}

					b = b << 1;
					j--;
				}
			}
			if (buf[pktlen] != ((crc >> 8) & 0xff) ||
			    buf[pktlen + 1] != (crc & 0xff)) {
				drop = true;
			}
			//FIXME: if var length, length field should be corrected.
		}
	}
#endif

	// Add to ring buffer
	if (!drop && ring_buf_bytes_free(rx_buf) >= hdrlen + pktlen) {
		ring_buf_add(rx_buf, buf, hdrlen + pktlen);
	} else {
		DBG_PRINTF(DBG_LVL_LOW, "Dropping packet: %s\n", drop ? "CRC error" : "RX buffer overflow");
	}

	return ERR_OK;
fail:
	return err;
}

static void _dump_status(rf_dev_t *dev)
{
	uint8_t buf[2];

	spi_read_regs(dev->fd, RegIrqFlags1, buf, 2);
	printf("Interrupt Flags: %.2x %.2x\n", buf[0], buf[1]);
}

#define SX1231_FSTEP 61 // Depends on Oscillator frequency!!!
const char * const lna_values[8] = {
	"??????",
	"  Max.",
	" -6 dB",
	"-12 dB",
	"-24 dB",
	"-36 dB",
	"-48 dB",
	"??????",
};

static void _dump_packet_status(rf_dev_t *dev)
{
	uint8_t buf[6];
	int16_t afc;
	int16_t fei;
	uint8_t lna;
	uint8_t temp;

	spi_read_regs(dev->fd, RegAfcMsb, buf, 6);
	spi_read_reg(dev->fd, RegTemp2, &temp);
	spi_read_reg(dev->fd, RegLna, &lna);

	afc = (buf[0] << 8 | buf[1]) * SX1231_FSTEP;
	fei = (buf[2] << 8 | buf[3]) * SX1231_FSTEP;
	printf("AFC: %7d Hz, FEI: %7d Hz, LNA: %s, RSSI: -%u%s dB, Temp: %u C\n",
			afc, fei,
			lna_values[(lna >> 3) & 0x7],
			buf[5] >> 1, buf[5] & 1 ? ".5" : ".0",
			temp);
	//TODO: dump RegLna.LnaCurrentGain ??
}
