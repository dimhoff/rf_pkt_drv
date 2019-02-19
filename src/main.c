/**
 * main.c - Packetized radio transceiver user space driver
 *
 * Copyright (c) 2019, David Imhoff <dimhoff.devel@gmail.com>
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

#include "error.h"
#include "ring_buf.h"
#include "sparse_buf.h"
#include "parse_reg_file.h"
#include "debug.h"

#ifdef RF_BACKEND_SX1231
# include "sx1231.h"
#else
# include "si443x.h"
#endif

#define RING_BUFFER_SIZE 4096

unsigned int debug_level = 0;

volatile sig_atomic_t terminate = 0;

void terminate_cb(int signum)
{
	terminate = 1;
}

void usage(const char *name)
{
	fprintf(stderr,
		"Packetized Radio Transceiver User Space driver - " VERSION "\n"
		"Usage: %s [-v] [-d <device>] [-c <config>] [-s <socket>] [-i <gpio#>]\n"
		"\n"
		"Options:\n"
		" -c <path>	Register Configuration file\n"
		" -d <path>	SPI device file to use (default: " DEFAULT_DEV_PATH ")\n"
		" -s <path>	Socket path for clients (default: " DEFAULT_SOCK_PATH ")\n"
		" -i <gpio#>	IRQ GPIO pin number, or -1 to use polling (default: %d)\n"
		" -v		Increase verbosity level, use multiple times for more logging\n"
		" -h		Display this help message\n",
		name, DEFAULT_IRQ_PIN);
}

int main(int argc, char *argv[])
{
	char *dev_path = DEFAULT_DEV_PATH;
	char *sock_path = DEFAULT_SOCK_PATH;
	char *cfg_path = DEFAULT_CFG_PATH;

	int opt;
	int retval = EXIT_FAILURE;
	int r;
	int err;

	int gpio_pin = DEFAULT_IRQ_PIN;
	int gpio_fd = -1;

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

	rf_dev_t dev;

	ring_buf_t rx_data;
	ring_buf_t tx_data;
	sparse_buf_t regs;

	/************************ Argument Parsing **************************/
	while ((opt = getopt(argc, argv, "hd:c:s:i:v")) != -1) {
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
		case 'i': {
			char *endp;
			gpio_pin = strtol(optarg, &endp, 10);
			if (*endp != '\0') {
				fprintf(stderr, "IRQ pin number must be a integer number.\n");
				exit(EXIT_FAILURE);
			} else if (gpio_pin >= 1000) {
				fprintf(stderr, "IRQ pin number < 1000 are currently only supported.\n");
				exit(EXIT_FAILURE);
			}
			break;
		}
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
			debug_level++;
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

	/************************** Initialization **************************/
	sparse_buf_init(&regs, 0x80);
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

	if (signal(SIGINT, &terminate_cb) == SIG_IGN) {
		signal(SIGINT, SIG_IGN);
	}
	if (signal(SIGHUP, &terminate_cb) == SIG_IGN) {
		signal(SIGHUP, SIG_IGN);
	}
	if (signal(SIGTERM, &terminate_cb) == SIG_IGN) {
		signal(SIGTERM, SIG_IGN);
	}
	signal(SIGPIPE, SIG_IGN);

	// Initialize buffers
	ring_buf_init(&rx_data, RING_BUFFER_SIZE);
	ring_buf_init(&tx_data, RING_BUFFER_SIZE);

	// Setup server socket
	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		goto cleanup2;
	}

	local.sun_family = AF_UNIX;
	assert(strlen(sock_path) < sizeof(local.sun_path)+1);
	strcpy(local.sun_path, sock_path);
	unlink(local.sun_path);
	if (bind(sock_fd, (struct sockaddr *)&local,
			strlen(local.sun_path) + sizeof(local.sun_family)) == -1) {
		perror("bind");
		goto cleanup2;
	}

	if (listen(sock_fd, 5) == -1) {
		perror("listen");
		goto cleanup2;
	}

	if (chmod(sock_path, 0777) != 0) {
		perror("chmod");
		goto cleanup2;
	}

	// Setup Transceiver device
	if (rf_open(&dev, dev_path) != 0) {
		perror("rf_open()");
		goto cleanup2;
	}

	if (rf_init(&dev, &regs) != 0) {
		fprintf(stderr, "Failed to initialize transceiver\n");
		goto cleanup;
	}

	// Setup interrupt pin
	if (gpio_pin >= 0) {
#define GPIO_SYS_PATH "/sys/class/gpio"
#define GPIO_INT_EDGE "rising"
		char path_buf[sizeof(GPIO_SYS_PATH"/gpio999/direction")];

		// configure direction
		snprintf(path_buf, sizeof(path_buf), GPIO_SYS_PATH"/gpio%u/direction", gpio_pin);
		if ((gpio_fd = open(path_buf, O_WRONLY)) == -1) {
			perror(path_buf);
			goto cleanup;
		}
		if (write(gpio_fd, "in", 2) != 2) {
			perror("Failed to configure IRQ GPIO pin direction");
			close(gpio_fd);
			goto cleanup;
		}
		close(gpio_fd);

		// configure trigger edge
		snprintf(path_buf, sizeof(path_buf), GPIO_SYS_PATH"/gpio%u/edge", gpio_pin);
		if ((gpio_fd = open(path_buf, O_WRONLY)) == -1) {
			perror(path_buf);
			goto cleanup;
		}
		if (write(gpio_fd, GPIO_INT_EDGE, strlen(GPIO_INT_EDGE)) != strlen(GPIO_INT_EDGE)) {
			perror("Failed to configure IRQ GPIO trigger edge");
			close(gpio_fd);
			goto cleanup;
		}
		close(gpio_fd);

		// open value file
		snprintf(path_buf, sizeof(path_buf), GPIO_SYS_PATH"/gpio%u/value", gpio_pin);
		if ((gpio_fd = open(path_buf, O_RDONLY)) == -1) {
			perror(path_buf);
			goto cleanup;
		}
	}

	/*************************** Main loop ******************************/
	for (;;) {
		// Setup select structures
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);

		nfds = sock_fd;
		FD_SET(sock_fd, &rfds);

		if (client_fd != -1) {
			if (client_fd > nfds) {
				nfds = client_fd;
			}
			if (! ring_buf_empty(&rx_data)) {
				FD_SET(client_fd, &wfds);
			}
			if (! ring_buf_full(&tx_data)) {
				FD_SET(client_fd, &rfds);
			}
		}
		if (gpio_fd != -1) {
			FD_SET(gpio_fd, &efds);
			if (gpio_fd > nfds) {
				nfds = gpio_fd;
			}
		}

		timeout.tv_sec = 1;
		timeout.tv_nsec = 0;

		// Wait for events
		r = pselect(nfds + 1, &rfds, &wfds, &efds, &timeout,
			    &empty_mask);
		if (r == -1 && errno != EINTR) {
			perror("select()");
			goto cleanup;
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
					DBG_PRINTF(DBG_LVL_LOW, "Closing old connection in favor of new one\n");
					close(client_fd);
					client_fd = -1;
				}

				// Accept new client
				t = sizeof(remote);
				if ((client_fd = accept(sock_fd, (struct sockaddr *)&remote, &t)) == -1) {
					perror("accept");
					continue;
				}
				DBG_PRINTF(DBG_LVL_LOW, "Accepted new client connection\n");
				ring_buf_clear(&rx_data);
				ring_buf_clear(&tx_data);
			} else if (client_fd != -1) {
				if (FD_ISSET(client_fd, &rfds)) {
					// Read client socket
					ssize_t rlen;
					uint8_t rdbuf[1024];

					// TODO: loop to read every thing till buffer is full?
					rlen = ring_buf_bytes_free(&tx_data);
					if (rlen > sizeof(rdbuf)) {
						rlen = sizeof(rdbuf);
					}
					rlen = read(client_fd, rdbuf, rlen);
					if (rlen <= 0) {
						if (rlen < 0) {
							perror("Client read failure");
						} else {
							DBG_PRINTF(DBG_LVL_LOW, "Client disconnected\n");
						}
						close(client_fd);
						client_fd = -1;
						continue;
					}

					ring_buf_add(&tx_data, rdbuf, rlen);
					DBG_PRINTF(DBG_LVL_HIGH, "Read client %zd bytes\n", rlen);
				}
				if (FD_ISSET(client_fd, &wfds)) {
					// Write client socket
					ssize_t wlen;

					wlen = write(client_fd, ring_buf_begin(&rx_data),
						     ring_buf_bytes_readable(&rx_data));
					if (wlen == -1) {
						perror("Client write failure");
						close(client_fd);
						client_fd = -1;
						continue;
					}
					ring_buf_consume(&rx_data, wlen);
					DBG_PRINTF(DBG_LVL_HIGH, "Written client %zd bytes\n", wlen);
				}
			}
			if (gpio_fd != -1 && FD_ISSET(gpio_fd, &efds)) {
				// clear readable status, but we don't care about the data
				char rdbuf[10];
				lseek(gpio_fd, 0, SEEK_SET);
				if (read(gpio_fd, rdbuf, sizeof(rdbuf)) < 0) {
					if (errno != EINTR) {
						perror("Error reading from interrupt pin");
						goto cleanup;
					}
				}
				DBG_PRINTF(DBG_LVL_HIGH, "Interrupt Requested\n");
			}
		}

		err = rf_handle(&dev, &rx_data, &tx_data);
		if (err == ERR_RFM_TX_OUT_OF_SYNC) {
			fprintf(stderr, "TX buffer out-of-sync, Disconnecting client\n");
			close(client_fd);
			client_fd = -1;
		} else if (err != ERR_OK) {
			char err_buf[sizeof("ERROR: 0x00112233")];
			snprintf(err_buf, sizeof(err_buf), "ERROR: 0x%08x", err);
			if (ERROR_ERRNO_VALID(err)) {
				perror(err_buf);
			} else {
				fprintf(stderr, "%s\n", err_buf);
			}
			goto cleanup;
		}
	}

	retval = EXIT_SUCCESS;
cleanup:
	if (gpio_fd != -1) {
		close(gpio_fd);
	}
	rf_close(&dev);
cleanup2:
	if (client_fd != -1) {
		close(client_fd);
	}
	if (sock_fd != -1) {
		close(sock_fd);
		unlink(local.sun_path);
	}

	ring_buf_destroy(&rx_data);
	ring_buf_destroy(&tx_data);

	return retval;
}
