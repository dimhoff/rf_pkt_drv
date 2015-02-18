/**
 * test_parse_reg_file.c - Unit test for parse_reg_file.c
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
#include <assert.h>
#include <unistd.h>

#include <check.h>

#include "sparse_buf.h"
#include "parse_reg_file.h"
#include "recursive_rmdir.h"

char testdir[] = "/tmp/test_parse_reg_file.XXXXXX";

const char *create_config(const char *content)
{
	FILE *fp;
	const char *name = "test.config";
	if ((fp = fopen(name, "wb")) == NULL) {
		perror("create test config");
		return NULL;
	}
	assert(fputs(content, fp) != EOF);
	fclose(fp);
	return name;
}

/**
 * Test parsing simple configuration
 */
START_TEST(test_simple_addr_val)
{
	const char *test_filename = NULL;
	sparse_buf_t buf;

	// Create reg file
	assert(sparse_buf_init(&buf, 0x80) == 0);
	test_filename = create_config(
		"00 11\n"
		"01 22\n"
		);
	assert(test_filename != NULL);

	// Parse reg file
	ck_assert_int_eq(parse_reg_file(test_filename, &buf), 0);

	// Verify read data
	ck_assert(sparse_buf_is_valid(&buf, 0) == true);
	ck_assert_uint_eq(*sparse_buf_at(&buf, 0), 0x11);
	ck_assert(sparse_buf_is_valid(&buf, 1) == true);
	ck_assert_uint_eq(*sparse_buf_at(&buf, 1), 0x22);
	ck_assert_uint_eq(sparse_buf_next_valid(&buf, 2), SPARSE_BUF_OFF_END);

	sparse_buf_destroy(&buf);
}
END_TEST

/**
 * Test parsing simple configuration in WDS format
 */
START_TEST(test_simple_wds)
{
	const char *test_filename = NULL;
	sparse_buf_t buf;

	// Create reg file
	assert(sparse_buf_init(&buf, 0x80) == 0);
	test_filename = create_config(
		"S2 8011\n"
		"s2 8122\n"
		);
	assert(test_filename != NULL);

	// Parse reg file
	ck_assert_int_eq(parse_reg_file(test_filename, &buf), 0);

	// Verify read data
	ck_assert(sparse_buf_is_valid(&buf, 0) == true);
	ck_assert_uint_eq(*sparse_buf_at(&buf, 0), 0x11);
	ck_assert(sparse_buf_is_valid(&buf, 1) == true);
	ck_assert_uint_eq(*sparse_buf_at(&buf, 1), 0x22);
	ck_assert_uint_eq(sparse_buf_next_valid(&buf, 2), SPARSE_BUF_OFF_END);

	sparse_buf_destroy(&buf);
}
END_TEST

/**
 * Test various valid configs
 */
const char *valid_configs[] = {
	// 0 - empty line
	"00 11\n"
	"\n"
	"01 22\n"
	, // 1 - empty line with space
	"00 11\n"
	" \t\n"
	"01 22\n"
	, // 2 - Leading space
	"  00 11\n"
	"\t\t01 22\n"
	, // 3 - Trailing space
	"00 11  \n"
	"01 22\t\t\n"
	, // 4 - Missing new line at end of file
	"00 11\n"
	"01 22"
};
START_TEST(loop_test_valid_configs)
{
	const char *test_filename = NULL;
	sparse_buf_t buf;

	// Create reg file
	assert(sparse_buf_init(&buf, 0x80) == 0);
	test_filename = create_config(valid_configs[_i]);
	assert(test_filename != NULL);

	// Parse reg file
	ck_assert_int_eq(parse_reg_file(test_filename, &buf), 0);

	// Verify read data
	ck_assert(sparse_buf_is_valid(&buf, 0) == true);
	ck_assert_uint_eq(*sparse_buf_at(&buf, 0), 0x11);
	ck_assert(sparse_buf_is_valid(&buf, 1) == true);
	ck_assert_uint_eq(*sparse_buf_at(&buf, 1), 0x22);
	ck_assert_uint_eq(sparse_buf_next_valid(&buf, 2), SPARSE_BUF_OFF_END);

	sparse_buf_destroy(&buf);
}
END_TEST

//TODO: broken configs: line length

/**
 * Generate test suite for sparse buffer
 */
Suite * parse_reg_file_suite(void)
{
	Suite *s;
	TCase *tc_simple;
	TCase *tc_valid;

	s = suite_create("parse_reg_file");

	// Simple parsing
	tc_simple = tcase_create("simple");
	tcase_add_test(tc_simple, test_simple_addr_val);
	tcase_add_test(tc_simple, test_simple_wds);
	suite_add_tcase(s, tc_simple);

	// Valid configs
	tc_valid = tcase_create("valid");
	tcase_add_loop_test(tc_valid, loop_test_valid_configs, 0,
			sizeof(valid_configs) / sizeof(valid_configs[0]));
	suite_add_tcase(s, tc_valid);

	return s;
}

/**
 * Main function
 *
 * Invokes the Check unit test framework test suites
 */
int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	if (mkdtemp(testdir) == NULL) {
		perror("Failed to create temporary directory");
		exit(EXIT_FAILURE);
	}
	if (chdir(testdir) != 0) {
		perror("chdir()");
		exit(EXIT_FAILURE);
	}

	s = parse_reg_file_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);

	srunner_free(sr);
	recursive_rmdir(testdir);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
