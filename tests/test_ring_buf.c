/**
 * test_ring_buf.c - Unit test for ring_buf.c
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

#include "ring_buf.h"

START_TEST(test_create)
{
	ring_buf_t buf;
	ring_buf_init(&buf, 4);
	ck_assert_uint_eq(ring_buf_size(&buf), 4);
	ck_assert_uint_eq(ring_buf_bytes_used(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 3);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 3);
	ck_assert(ring_buf_empty(&buf) == true);
	ring_buf_destroy(&buf);
}
END_TEST

START_TEST(test_simple_add)
{
	ring_buf_t buf;
	uint8_t data[] = { 0x11 };

	ring_buf_init(&buf, 4);
	ring_buf_add(&buf, data, sizeof(data));
	ck_assert_uint_eq(ring_buf_size(&buf), 4);
	ck_assert_uint_eq(ring_buf_bytes_used(&buf), 1);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 2);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), 1);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 2);
	ck_assert(ring_buf_empty(&buf) == false);
	ck_assert(ring_buf_begin(&buf)[0] == 0x11);
	ring_buf_destroy(&buf);
}
END_TEST

START_TEST(test_simple_full_consume)
{
	ring_buf_t buf;
	uint8_t data[] = { 0x11 };

	ring_buf_init(&buf, 4);
	ring_buf_add(&buf, data, sizeof(data));
	ring_buf_consume(&buf, 1);

	ck_assert_uint_eq(ring_buf_size(&buf), 4);
	ck_assert_uint_eq(ring_buf_bytes_used(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 3);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 3);
	ck_assert(ring_buf_empty(&buf) == true);

	ring_buf_destroy(&buf);
}
END_TEST

START_TEST(test_simple_partial_consume)
{
	ring_buf_t buf;
	uint8_t data[] = { 0x11, 0x22 };

	ring_buf_init(&buf, 4);
	ring_buf_add(&buf, data, sizeof(data));
	ring_buf_consume(&buf, 1);

	ck_assert_uint_eq(ring_buf_size(&buf), 4);
	ck_assert_uint_eq(ring_buf_bytes_used(&buf), 1);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 2);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), 1);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 2);
	ck_assert(ring_buf_empty(&buf) == false);
	ck_assert(ring_buf_begin(&buf)[0] == 0x22);

	ring_buf_destroy(&buf);
}
END_TEST

START_TEST(test_clear)
{
	ring_buf_t buf;
	uint8_t data[] = { 0x11, 0x22 };

	ring_buf_init(&buf, 4);
	ring_buf_add(&buf, data, sizeof(data));
	ring_buf_clear(&buf);

	ck_assert_uint_eq(ring_buf_size(&buf), 4);
	ck_assert_uint_eq(ring_buf_bytes_used(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 3);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 3);
	ck_assert(ring_buf_empty(&buf) == true);

	ring_buf_destroy(&buf);
}
END_TEST

START_TEST(test_wrap)
{
	ring_buf_t buf;
	uint8_t data[] = { 0x11, 0x22, 0x33, 0x44 };
	uint8_t data2[] = { 0x55, 0x66 };

	ring_buf_init(&buf, 5);

	// Move Read pointer
	// NOTE: leave one element in queue to prevent reseting of pointers on
	// empty queue
	// queue:    |DDDDF|
	// pointers: |r   w|
	ring_buf_add(&buf, data, sizeof(data));
	// queue:    |FFFDF|
	// pointers: |   rw|
	ring_buf_consume(&buf, sizeof(data)-1);

	// Wrap write pointer
	// queue:    |DFFDD|
	// pointers: | w r |
	ring_buf_add(&buf, data2, sizeof(data2));

	ck_assert_uint_eq(ring_buf_bytes_used(&buf), 3);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 1);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), 2);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 1);
	ck_assert(ring_buf_empty(&buf) == false);
	ck_assert(ring_buf_begin(&buf)[0] == 0x44);
	ck_assert(ring_buf_begin(&buf)[1] == 0x55);

	// Wrap read pointer
	// queue:    |DFFFF|
	// pointers: |rw   |
	ring_buf_consume(&buf, 2);

	ck_assert_uint_eq(ring_buf_bytes_used(&buf), 1);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 3);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), 1);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 3);
	ck_assert(ring_buf_empty(&buf) == false);
	ck_assert(ring_buf_begin(&buf)[0] == 0x66);

	ring_buf_destroy(&buf);
}
END_TEST

/**
 * Overflow buffer
 *
 * Expected: Bytes from the begining of the buffer are consumed to make space
 * for the new data.
 */
START_TEST(test_overflow)
{
	ring_buf_t buf;
	uint8_t data[] = { 0x11, 0x22, 0x33, 0x44 };
	uint8_t data2[] = { 0x55, 0x66 };
	const size_t queue_size = 5;

	ring_buf_init(&buf, queue_size);

	// Fill initial data
	// queue:    |DDDDF|
	// pointers: |r   w|
	ring_buf_add(&buf, data, sizeof(data));

	// Overflow buffer
	// queue:    |DFDDD|
	// pointers: | wr  |
	ring_buf_add(&buf, data2, sizeof(data2));

	ck_assert_uint_eq(ring_buf_bytes_used(&buf), queue_size - 1);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), 3);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 0);
	ck_assert(ring_buf_empty(&buf) == false);
	ck_assert(ring_buf_begin(&buf)[0] == 0x33);
	ck_assert(ring_buf_begin(&buf)[1] == 0x44);
	ck_assert(ring_buf_begin(&buf)[2] == 0x55);

	ring_buf_destroy(&buf);
}
END_TEST

/**
 * Overflow buffer with more than capacity amount of bytes
 *
 * Expected: Buffer is reset and only the last 'capacity' amount of bytes form
 * the new data are written at the start of the buffer.
 */
START_TEST(test_overflow_add)
{
	ring_buf_t buf;
	uint8_t data[] = { 0x11, 0x22 };
	uint8_t data2[] = { 0x33, 0x44, 0x55, 0x66, 0x77};
	const size_t queue_size = sizeof(data2);

	ring_buf_init(&buf, queue_size);

	// Fill initial data
	// queue:    |DDFFF|
	// pointers: |r w  |
	ring_buf_add(&buf, data, sizeof(data));

	// Overflow buffer
	// queue:    |DDDDF|
	// pointers: |r   w|
	ring_buf_add(&buf, data2, sizeof(data2));

	ck_assert_uint_eq(ring_buf_bytes_used(&buf), queue_size - 1);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), sizeof(data2) - 1);
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 0);
	ck_assert(ring_buf_empty(&buf) == false);
	ck_assert(memcmp(&data2[1], ring_buf_begin(&buf),
			 sizeof(data2) - 1) == 0);

	ring_buf_destroy(&buf);
}
END_TEST

/**
 * Overflow buffer with exactly capacity amount of bytes
 *
 * Expected: Buffer is reset and the new data is written from the start of the
 * buffer.
 */
START_TEST(test_overflow_add_eq_to_cap)
{
	ring_buf_t buf;
	uint8_t data[] = { 0x11, 0x22 };
	uint8_t data2[] = { 0x33, 0x44, 0x55, 0x66, 0x77};
	const size_t queue_size = sizeof(data2) + 1;

	ring_buf_init(&buf, queue_size);

	// Fill initial data
	// queue:    |DDFFF|
	// pointers: |r w  |
	ring_buf_add(&buf, data, sizeof(data));

	// Overflow buffer
	// queue:    |DDDDF|
	// pointers: |r   w|
	ring_buf_add(&buf, data2, sizeof(data2));

	ck_assert_uint_eq(ring_buf_bytes_used(&buf), queue_size - 1);
	ck_assert_uint_eq(ring_buf_bytes_available(&buf), 0);
	ck_assert_uint_eq(ring_buf_bytes_readable(&buf), sizeof(data2));
	ck_assert_uint_eq(ring_buf_bytes_writable(&buf), 0);
	ck_assert(ring_buf_empty(&buf) == false);
	ck_assert(memcmp(&data2[0], ring_buf_begin(&buf),
			 sizeof(data2)) == 0);

	ring_buf_destroy(&buf);
}
END_TEST

/**
 * Generate test suite for ring buffer
 */
Suite *ring_buf_suite(void)
{
	Suite *s;
	TCase *tc_create;
	TCase *tc_simple;
	TCase *tc_wrap;
	TCase *tc_overflow;

	s = suite_create("ring_buf");

	// Creation
	tc_create = tcase_create("creation");
	tcase_add_test(tc_create, test_create);
	suite_add_tcase(s, tc_create);

	// Add & Consume
	tc_simple = tcase_create("add_consume");
	tcase_add_test(tc_simple, test_simple_add);
	tcase_add_test(tc_simple, test_simple_full_consume);
	tcase_add_test(tc_simple, test_simple_partial_consume);
	tcase_add_test(tc_simple, test_clear);
	suite_add_tcase(s, tc_simple);

	// Wrapping
	tc_wrap = tcase_create("wrapping");
	tcase_add_test(tc_wrap, test_wrap);
	suite_add_tcase(s, tc_wrap);

	// Overflow
	tc_overflow = tcase_create("overflow");
	tcase_add_test(tc_overflow, test_overflow);
	tcase_add_test(tc_overflow, test_overflow_add);
	tcase_add_test(tc_overflow, test_overflow_add_eq_to_cap);
	suite_add_tcase(s, tc_overflow);

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

	s = ring_buf_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
