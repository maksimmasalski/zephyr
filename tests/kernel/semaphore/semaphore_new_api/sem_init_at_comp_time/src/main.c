/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Tests for the Semaphore kernel object
 * @defgroup kernel_semaphore_tests Semaphore
 * @ingroup all_tests
 * @{
 * @}
 */

#include <ztest.h>

/* Macro declarations */
#define SEM_INIT_COUNT (1U)
#define SEM_MAX_COUNT  (1U)

/******************************************************************************/
/* Kobject declaration
 * Semaphore definition and initialization during the compile time
 * It is set with value 1 ready to be taken by a thread, maximum count 1
 * */
K_SEM_DEFINE(simple_sem, SEM_INIT_COUNT, SEM_MAX_COUNT);

/**
 * @brief Test that semaphore was defined during the compile time
 * 
 * Description: This routine gets a semaphore count.
 * If semaphore count equal to 1, means that semaphore defined
 * and initialized successfully.
 * Input parameters: None
 * Return: None
 *
 * @see K_SEM_DEFINE
 * @ingroup kernel_semaphore_tests
 * @verify{@req{361}}
 */
void test_k_sem_define(void)
{
	u32_t sem_count;
	sem_count = k_sem_count_get(&simple_sem);
	zassert_true((sem_count == SEM_INIT_COUNT),
			"Semaphore didn't initialize at the compile time, "
			"because K_SEM_DEFINE failed -expected count %d, got %d",
			 SEM_INIT_COUNT, sem_count);
}

/* ztest main entry*/
void test_main(void)
{
	ztest_test_suite(test_semaphore,
			ztest_unit_test(test_k_sem_define));
	ztest_run_test_suite(test_semaphore);
}
/******************************************************************************/
