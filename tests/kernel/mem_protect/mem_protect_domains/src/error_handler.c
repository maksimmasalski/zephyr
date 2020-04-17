/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <ztest.h>

ZTEST_BMEM bool expected_fault;
#define ASSERT_BARRIER_SEMAPHORE_INIT_COUNT (0)
#define ASSERT_BARRIER_SEMAPHORE_MAX_COUNT (1)

K_SEM_DEFINE(assert_sem,
	     ASSERT_BARRIER_SEMAPHORE_INIT_COUNT,
	     ASSERT_BARRIER_SEMAPHORE_MAX_COUNT);
void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *pEsf)
{
	TC_PRINT("Caught system error -- reason %d\n", reason);
	if(reason == expected_fault) {

		/* reset back expected fault to normal */
		expected_fault = -1;
		ztest_test_pass();
	} else {
		k_fatal_halt(reason);
	}
}
void assert_post_action(const char *file, unsigned int line)
{
	ztest_test_pass();
}
