/**
 * test_sparse_buf.c - Unit test for sparse_buf.c
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

#include <check.h>

#include "sparse_buf.h"

START_TEST(test_create)
{
	sparse_buf_t buf;
	ck_assert_int_eq(sparse_buf_init(&buf, 5), 0);
	ck_assert_uint_eq(sparse_buf_size(&buf), 5);
	sparse_buf_destroy(&buf);
}
END_TEST

/**
 * Add some bytes in middle of buffer and check accessors
 *
 * Expected: Bytes are succesfully added to buffer and data accessors return
 * correct information.
 */
START_TEST(test_simple_write)
{
	int i;
	sparse_buf_t buf;
	ck_assert_int_eq(sparse_buf_init(&buf, 5), 0);

	ck_assert_int_eq(sparse_buf_write(&buf, 2, 0x11), 0);
	ck_assert_int_eq(sparse_buf_write(&buf, 3, 0x22), 0);
	for (i=0; i < 5; i++) {
		bool expected = false;
		if (i == 2 || i == 3)
			expected = true;
		ck_assert(sparse_buf_is_valid(&buf, i) == expected);
	}
	ck_assert_uint_eq(sparse_buf_next_valid(&buf, 0), 2);
	ck_assert_uint_eq(sparse_buf_next_valid(&buf, 2), 2);
	ck_assert_uint_eq(sparse_buf_next_invalid(&buf, 2), 4);
	ck_assert_uint_eq(sparse_buf_valid_length(&buf, 2), 2);
	//TODO: read byte from buffer

	sparse_buf_destroy(&buf);
}
END_TEST

/**
 * Test accessors on buffer without valid data
 *
 * Expected: accessors indicate that all data is invalid.
 */
START_TEST(test_all_invalid)
{
	int i;
	sparse_buf_t buf;
	ck_assert_int_eq(sparse_buf_init(&buf, 5), 0);

	for (i=0; i < 5; i++) {
		ck_assert(sparse_buf_is_valid(&buf, i) == false);
	}
	ck_assert_uint_eq(sparse_buf_next_valid(&buf, 0), SPARSE_BUF_OFF_END);
	ck_assert_uint_eq(sparse_buf_next_invalid(&buf, 0), 0);
	ck_assert_uint_eq(sparse_buf_valid_length(&buf, 0), 0);

	sparse_buf_destroy(&buf);
}
END_TEST

/**
 * Test accessors on buffer with all valid data
 *
 * Expected: accessors indicate that all data is valid.
 */
START_TEST(test_all_valid)
{
	int i;
	sparse_buf_t buf;
	ck_assert_int_eq(sparse_buf_init(&buf, 5), 0);

	for (i=0; i < 5; i++) {
		ck_assert_int_eq(sparse_buf_write(&buf, i, 0x30 + i), 0);
	}
	for (i=0; i < 5; i++) {
		ck_assert(sparse_buf_is_valid(&buf, i) == true);
	}
	ck_assert_uint_eq(sparse_buf_next_valid(&buf, 0), 0);
	ck_assert_uint_eq(sparse_buf_next_invalid(&buf, 0), SPARSE_BUF_OFF_END);
	ck_assert_uint_eq(sparse_buf_valid_length(&buf, 0), 5);

	sparse_buf_destroy(&buf);
}
END_TEST

/**
 * Test validity bitset consisting of multiple elements
 *
 * The validity bitset is an array of unsigned integers.
 * Expected: The code correctly handles a bitset consisting of more than 1
 * element.
 */


/**
 * Generate test suite for sparse buffer
 */
Suite *sparse_buffer_suite(void)
{
	Suite *s;
	TCase *tc_create;
	TCase *tc_simple;

	s = suite_create("sparse_buffer");

	// Creation
	tc_create = tcase_create("creation");
	tcase_add_test(tc_create, test_create);
	suite_add_tcase(s, tc_create);

	// Add & Consume
	tc_simple = tcase_create("accessors");
	tcase_add_test(tc_simple, test_simple_write);
	tcase_add_test(tc_simple, test_all_invalid);
	tcase_add_test(tc_simple, test_all_valid);
	suite_add_tcase(s, tc_simple);

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

	s = sparse_buffer_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
