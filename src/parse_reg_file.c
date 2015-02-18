/**
 * parse_reg_file.c - Parse register configuration file
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
#include "parse_reg_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "dehexify.h"

int parse_reg_file(const char *filename, sparse_buf_t *regs)
{
	FILE *fp = NULL;
	unsigned int line_nr = 0;
	char buf[1024];
	int retval = -1;
	int r;

	// Open file
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Unable to open file '%s': %s\n",
				filename, strerror(errno));
		return -1;
	}

	sparse_buf_clear(regs);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		size_t len = strlen(buf);
		char *bp = &buf[len - 1];
		uint8_t bin[4];
		uint8_t addr;
		uint8_t val;

		line_nr++;

		if (*bp != '\n' && ! feof(fp)) {
			fprintf(stderr, "%s:%u: Line too long\n",
					filename, line_nr);
			goto err;
		}

		// Strip trailing & leading white space
		while (len && (*bp == ' ' || *bp == '\t' || *bp == '\n')) {
			*bp = '\0';
			bp--;
			len--;
		}
		bp = buf;
		while (len && (*bp == ' ' || *bp == '\t')) {
			bp++;
			len--;
		}

		if (len == 0) {
			// Empty line
			continue;
		}

		// Parse line
		if (bp[0] == 'S' || bp[0] == 's') {
			// WDS set format (only limited subset of format)
			if (len != 7) {
				fprintf(stderr, "%s:%u: "
						"Invalid WDS line length\n",
						filename, line_nr);
				goto err;
			}
			if (bp[1] != '2' || bp[2] != ' ') {
				fprintf(stderr, "%s:%u: "
						"Incorrect WDS format\n",
						filename, line_nr);
				goto err;
			}

			r = dehexify(&bp[3], 2, bin);
			if (r != 0) {
				fprintf(stderr, "%s:%u: "
						"Invalid hex encoding\n",
						filename, line_nr);
				goto err;
			}

			if ((bin[0] & 0x80) == 0) {
				fprintf(stderr, "%s:%u: Expecting MSB of "
					"address to be 1 in WDS format\n",
					filename, line_nr);
				goto err;
			}
		} else {
			// addr,value format
			if (len != 5) {
				fprintf(stderr, "%s:%u: "
						"Invalid WDS line length\n",
						filename, line_nr);
				goto err;
			}
			if (bp[2] != ' ') {
				fprintf(stderr, "%s:%u: "
						"Incorrect seperator\n",
						filename, line_nr);
				goto err;
			}

			r = dehexify(&bp[0], 1, &bin[0]);
			if (r != 0) {
				fprintf(stderr, "%s:%u: Invalid hex encoding "
						"of address '%.2s'\n",
						filename, line_nr, &bp[0]);
				goto err;
			}
			r = dehexify(&bp[3], 1, &bin[1]);
			if (r != 0) {
				fprintf(stderr, "%s:%u: "
					"Invalid hex encoding of value '%.2s'\n",
					filename, line_nr, &bp[0]);
				goto err;
			}

			if ((bin[0] & 0x80) != 0) {
				fprintf(stderr, "%s:%u: Expecting MSB of "
					"address to be 0 in addr,val format\n",
					filename, line_nr);
				goto err;
			}
		}

		// Process register
		addr = bin[0] & 0x7F;
		val = bin[1];

		if (addr == 0x7F) {
			fprintf(stderr, "%s:%u: "
					"Illegal register address 0x7F\n",
					filename, line_nr);
			goto err;
		}

		if (sparse_buf_write(regs, addr, val) != 0) {
			fprintf(stderr, "%s:%u Unable to add register at "
					"address %hhu to buffer\n",
					filename, line_nr, addr);
			goto err;
		}
	}
	if (!feof(fp)) {
		fprintf(stderr, "An error occurred reading file\n");
		goto err;
	}

	retval = 0;
err:
	fclose(fp);

	return retval;
}
