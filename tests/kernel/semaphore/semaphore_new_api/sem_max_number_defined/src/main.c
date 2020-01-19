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

/* Current stack size for Ztest is 512 bytes,
 * so 39 semaphores is max for the reel board */
#define MAX_COUNT 39U


/**
 * @brief Test application can define any number of semaphores
 * @see k_sem *sem
 * @ingroup kernel_semaphore_tests
 * @verify{@req{357}}
 */
void test_k_sem_max_number(void)
{
	u32_t i;
	u32_t sem_num = 0;
	struct k_sem sem_array[MAX_COUNT];

	for(i=0U; i<MAX_COUNT; i++)
	{
		sem_num += 1;
		k_sem_init(&sem_array[i], 1, 1);
		printk("Created semaphore #%d\n", sem_num);
	}
	zassert_true((sem_num == MAX_COUNT),
			"Max number of the created semaphores not reached, "
			"real number of created semaphores is %d, expected %d",
			 sem_num, MAX_COUNT);
}

/* ztest main entry*/
void test_main(void)
{
	ztest_test_suite(test_semaphore_max_number,
			ztest_unit_test(test_k_sem_max_number));
	ztest_run_test_suite(test_semaphore_max_number);
}
