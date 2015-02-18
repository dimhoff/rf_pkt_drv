/**
 * test_dehexify.c - Unit test for dehexify.c
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

#include "dehexify.h"

START_TEST(test_all_hex)
{
	const char *in =
		"000102030405060708090a0b0c0d0e0f"
		"101112131415161718191a1b1c1d1e1f"
		"202122232425262728292a2b2c2d2e2f"
		"303132333435363738393a3b3c3d3e3f"
		"404142434445464748494a4b4c4d4e4f"
		"505152535455565758595a5b5c5d5e5f"
		"606162636465666768696a6b6c6d6e6f"
		"707172737475767778797a7b7c7d7e7f"
		"808182838485868788898a8b8c8d8e8f"
		"909192939495969798999a9b9c9d9e9f"
		"a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
		"b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
		"c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
		"d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
		"e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
		"f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
	uint8_t out[0x100];
	int err;
	unsigned int i;

	err = dehexify(in, sizeof(out), out);
	ck_assert_int_eq(err, 0);
	for (i=0; i < sizeof(out); i++) {
		ck_assert_uint_eq(out[i], i);
	}
}
END_TEST

START_TEST(test_case)
{
	const char *in = "AAaa";
	uint8_t out[2];
	int err;

	err = dehexify(in, sizeof(out), out);
	ck_assert_int_eq(err, 0);
	ck_assert_uint_eq(out[0], 0xaa);
	ck_assert_uint_eq(out[1], 0xaa);
}
END_TEST

START_TEST(test_too_short)
{
	const char *in = "00112";
	uint8_t out[3];
	int err;

	err = dehexify(in, sizeof(out), out);
	ck_assert_int_eq(err, -1);
}
END_TEST

START_TEST(test_longer)
{
	const char *in = "00112233";
	uint8_t out[3];
	int err;
	unsigned int i;

	err = dehexify(in, sizeof(out), out);
	ck_assert_int_eq(err, 0);
	for (i=0; i < sizeof(out); i++) {
		ck_assert_uint_eq(out[i], (i << 4) | (i & 0xf));
	}
}
END_TEST

START_TEST(test_empty)
{
	const char *in = "";
	uint8_t out[0];
	int err;

	err = dehexify(in, sizeof(out), out);
	ck_assert_int_eq(err, 0);
}
END_TEST

START_TEST(check_is_illegal)
{
	char in[2] = "00";
	uint8_t out[1];
	int err;

	in[1] = _i;
	err = dehexify(in, sizeof(out), out);
	ck_assert_int_eq(err, -1);
}
END_TEST

/**
 * Generate test suite for ring buffer
 */
Suite *dehexify_suite(void)
{
	Suite *s;
	TCase *tc_basic;
	TCase *tc_length;
	TCase *tc_illegal;

	s = suite_create("dehexify");

	// basic
	tc_basic = tcase_create("basic");
	tcase_add_test(tc_basic, test_all_hex);
	tcase_add_test(tc_basic, test_case);
	suite_add_tcase(s, tc_basic);

	// length check
	tc_length = tcase_create("length");
	tcase_add_test(tc_length, test_too_short);
	tcase_add_test(tc_length, test_longer);
	tcase_add_test(tc_length, test_empty);
	suite_add_tcase(s, tc_length);

	// illegal characters
	tc_illegal = tcase_create("illegal");
	tcase_add_loop_test(tc_illegal, check_is_illegal, 1, '0');
	tcase_add_loop_test(tc_illegal, check_is_illegal, '9'+1, 'A');
	tcase_add_loop_test(tc_illegal, check_is_illegal, 'Z'+1, 'a');
	tcase_add_loop_test(tc_illegal, check_is_illegal, 'z'+1, 0x100);
	suite_add_tcase(s, tc_illegal);

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

	s = dehexify_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
