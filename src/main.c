/**
 * main.c - Si443x transceiver user space driver
 *
 * Copyright (c) 2014, David Imhoff <dimhoff.devel@gmail.com>
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
#include "config.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <sys/select.h>

#include "ring_buf.h"
#include "sparse_buf.h"
#include "parse_reg_file.h"
#include "si443x.h"

#define RING_BUFFER_SIZE 4096

unsigned int verbose = 0;

volatile sig_atomic_t terminate = 0;

void terminate_cb(int signum)
{
	terminate = 1;
}

void usage(const char *name)
{
	fprintf(stderr,
		"Si443x User Space driver - " VERSION "\n"
		"Usage: %s [-v] [-d <device>] [-c <config>] [-s <socket>]\n"
		"\n"
		"Options:\n"
		" -c <path>	Register Configuration file\n"
		" -d <path>	SPI device file to use (default: " DEFAULT_DEV_PATH ")\n"
		" -s <path>	Socket path for clients (default: " DEFAULT_SOCK_PATH ")\n"
		" -v		Increase verbosity level, use multiple times for more logging\n"
		" -h		Display this help message\n",
		name);
}

#define CHECK(X) \
	do { \
		err = X; \
		if (err != 0) goto fail;\
	} while(0)

static int initialize_receiver(si443x_dev_t *dev, sparse_buf_t *regs)
{
	int err = -1;

	// reset
	CHECK(si443x_reset(dev));

	// Program magic values from calculator sheet
	CHECK(si443x_configure(dev, regs));

	// enable receiver in multi packet FIFO mode
	//TODO: use defines
	CHECK(si443x_write_reg(dev, OPERATING_MODE_AND_FUNCTION_CONTROL_1, 0x05));
	CHECK(si443x_write_reg(dev, OPERATING_MODE_AND_FUNCTION_CONTROL_2, 0x10));

	err = 0;
fail:
	return err;
}

int receive_frame(si443x_dev_t *dev, ring_buf_t *rbuf)
{
	uint8_t buf[64];
	uint8_t hdrlen;
	uint8_t pktlen;
	uint8_t val;
	int i;

	// Check if packet available
	si443x_read_reg(dev, DEVICE_STATUS, &val);
	if ((val & DEVICE_STATUS_RXFFEM)) {
		return 0;
	}

	if (verbose)
		si443x_dump_status(dev);

	// Wait till done receiving current packet
	//NOTE: DEVICE_STATUS.RXFFEM is also != 1 for partial packets!
	si443x_read_reg(dev, INTERRUPT_STATUS_2, &val);
	while ((val & INTERRUPT_STATUS_2_ISWDET)) {
		si443x_read_reg(dev, INTERRUPT_STATUS_2, &val);
		//TODO: add timeout?
	}

	// Read Header
	hdrlen = dev->txhdlen;
	if (dev->fixpklen == 0) {
		hdrlen += 1;
	}
	if (hdrlen) {
		si443x_read_regs(dev, FIFO_ACCESS, buf, hdrlen);
		if (verbose) {
			printf("Received header: \n");
			for (i = 0; i < hdrlen; i++) {
				printf("%.2x ", buf[i]);
			}
			putchar('\n');
			si443x_dump_status(dev);
		}
	}
	if (dev->fixpklen == 0) {
		pktlen = buf[hdrlen - 1];
		if (pktlen > SI443X_FIFO_SIZE - 3) {
			fprintf(stderr, "ERROR: Packet len too big (%.2x)\n",
				pktlen);
			goto err;
		}
	} else {
		pktlen = dev->fixpklen;
	}

	// Read Payload
	si443x_read_regs(dev, FIFO_ACCESS, &buf[hdrlen], pktlen);
	if (verbose) {
		printf("Received packet: \n");
		for (i = 0; i < pktlen; i++) {
			printf("%.2x ", buf[hdrlen + i]);
		}
		putchar('\n');

		si443x_dump_status(dev);
	}

	// Check FIFO over/underflow condition
	si443x_read_reg(dev, DEVICE_STATUS, &val);
	if (val & (DEVICE_STATUS_FFOVFL |
			DEVICE_STATUS_FFUNFL)) {
		fprintf(stderr, "ERROR: Device "
			"overflow/underflow (%.2x)\n", val);
		goto err;
	}

	// Add to ring buffer
	if (ring_buf_bytes_available(rbuf) >= hdrlen + pktlen)
		ring_buf_add(rbuf, buf, hdrlen + pktlen);

	return 0;
err:
	// Error Recovery
	//TODO: verify SPI is still working
	if (verbose)
		printf("resetting RX fifo\n");
	si443x_reset_rx_fifo(dev);
	return -1;
}

int main(int argc, char *argv[])
{
	char *dev_path = DEFAULT_DEV_PATH;
	char *sock_path = DEFAULT_SOCK_PATH;
	char *cfg_path = DEFAULT_CFG_PATH;

	int opt;
	int retval = EXIT_SUCCESS;
	int r;

	int sock_fd = -1;
	int client_fd = -1;
	struct sockaddr_un local;

	sigset_t sigmask;
	sigset_t empty_mask;
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	int nfds;
	struct timespec timeout;

	si443x_dev_t dev;

	ring_buf_t rx_data;
	sparse_buf_t regs;

	ring_buf_init(&rx_data, RING_BUFFER_SIZE);
	sparse_buf_init(&regs, 0x80);

	//TODO: split into smaller functions!!!

	// Argument parsing
	while ((opt = getopt(argc, argv, "hd:c:s:v")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'c':
			//TODO: this is currently not really an option, more an argument
			cfg_path = optarg;
			break;
		case 'd':
			dev_path = optarg;
			break;
		case 's':
			sock_path = optarg;
			if (strlen(sock_path) >= sizeof(local.sun_path)+1) {
				fprintf(stderr, "Socket path too long "
					"(max=%zu)\n",
					sizeof(local.sun_path)+1);
				exit(EXIT_FAILURE);
			}
			break;
		case 'v':
			verbose++;
			break;
		default: /* '?' */
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind != 0) {
		fprintf(stderr, "Additional arguments found\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (cfg_path == NULL) {
		fprintf(stderr, "No register configuration file specified\n");
		exit(EXIT_FAILURE);
	}

	if (parse_reg_file(cfg_path, &regs) != 0) {
		exit(EXIT_FAILURE);
	}

	// Setup signal handlers
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGHUP);
	sigaddset(&sigmask, SIGTERM);
	if (sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
		perror("sigprocmask");
		exit(EXIT_FAILURE);
	}

	sigemptyset(&empty_mask);

	if (signal(SIGINT, &terminate_cb) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, &terminate_cb) == SIG_IGN)
		signal(SIGHUP, SIG_IGN);
	if (signal(SIGTERM, &terminate_cb) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	// Setup server socket
	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		retval = EXIT_FAILURE;
		goto cleanup2;
	}

	local.sun_family = AF_UNIX;
	assert(strlen(sock_path) < sizeof(local.sun_path)+1);
	strcpy(local.sun_path, sock_path);
	unlink(local.sun_path);
	if (bind(sock_fd, (struct sockaddr *)&local,
			strlen(local.sun_path) + sizeof(local.sun_family)) == -1) {
		perror("bind");
		retval = EXIT_FAILURE;
		goto cleanup2;
	}

	if (listen(sock_fd, 5) == -1) {
		perror("listen");
		retval = EXIT_FAILURE;
		goto cleanup2;
	}

	if (chmod(sock_path, 0777) != 0) {
		perror("chmod");
		retval = EXIT_FAILURE;
		goto cleanup2;
	}

	// Setup Si443x device
	if (si443x_open(&dev, dev_path) != 0) {
		perror("si443x_open()");
		goto cleanup2;
	}

	if (initialize_receiver(&dev, &regs) != 0) {
		fprintf(stderr, "Failed to initialize Si443x device\n");
		goto cleanup2;
	}

	// Main loop
	for (;;) {
		// Setup select structures
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);

		nfds = sock_fd;
		FD_SET(sock_fd, &rfds);

		if (client_fd != -1) {
			if (client_fd > nfds)
				nfds = client_fd;
			if (! ring_buf_empty(&rx_data))
				FD_SET(client_fd, &wfds);
			FD_SET(client_fd, &rfds);
		}
		// TODO: interrupt line should be connected to GPIO, and gpio file handle should be in select

		timeout.tv_sec = 1;
		timeout.tv_nsec = 0;

		// Wait for events
		r = pselect(nfds + 1, &rfds, &wfds, &efds, &timeout,
			    &empty_mask);
		if (r == -1 && errno != EINTR) {
			perror("select()");
			break;
		}

		if (terminate) {
			break;
		}

		// Service sockets
		if (r > 0) {
			if (FD_ISSET(sock_fd, &rfds)) {
				struct sockaddr_un remote;
				socklen_t t;

				// Disconnect previous client
				if (client_fd != -1) {
					printf("Closing old connection in favor of new one\n");
					close(client_fd);
					client_fd = -1;
				}

				// Accept new client
				t = sizeof(remote);
				if ((client_fd = accept(sock_fd, (struct sockaddr *)&remote, &t)) == -1) {
					perror("accept");
					continue;
				}
				printf("Accepted new client connection\n");
				ring_buf_clear(&rx_data);
			} else if (client_fd != -1) {
				if (FD_ISSET(client_fd, &rfds)) {
					// Read client socket
					ssize_t rlen;
					char rdbuf[1024];
					rlen = read(client_fd, rdbuf, sizeof(rdbuf));
					if (rlen <= 0) {
						if (rlen < 0) {
							perror("Client read failure");
						} else {
							printf("Client disconnected\n");
						}
						close(client_fd);
						client_fd = -1;
						continue;
					}
					// TODO: do something with the received data....
					printf("Read client %zd bytes\n", rlen);
				}
				if (FD_ISSET(client_fd, &wfds)) {
					// Write client socket
					ssize_t wlen;
					printf("Write client\n");

					wlen = write(client_fd, ring_buf_begin(&rx_data),
						     ring_buf_bytes_readable(&rx_data));
					if (wlen == -1) {
						perror("Client write failure");
						close(client_fd);
						client_fd = -1;
						continue;
					}
					ring_buf_consume(&rx_data, wlen);
				}
			}
		}

		receive_frame(&dev, &rx_data);
	}

	si443x_close(&dev);
cleanup2:
	if (client_fd != -1) {
		close(client_fd);
	}
	if (sock_fd != -1) {
		close(sock_fd);
		unlink(local.sun_path);
	}

	return retval;
}
