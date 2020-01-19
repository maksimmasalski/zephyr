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
#include <irq_offload.h>

/* Macro declarations */
#define SEM_INIT_VAL (0U)
#define SEM_MAX_VAL  (10U)
#define sem_give_from_isr(sema) irq_offload(isr_sem_give, sema)

/******************************************************************************/
/* Kobject declaration */
K_SEM_DEFINE(simple_sem, SEM_INIT_VAL, SEM_MAX_VAL);

/******************************************************************************/
/* Helper functions */
void isr_sem_give(void *semaphore)
{
	k_sem_give((struct k_sem *)semaphore);
}

/**
 * @brief Test semaphore count when given by an ISR
 * @see irq_offload()
 * @ingroup kernel_semaphore_tests
 * @verify{@req{363}}
 */
void test_simple_sem_from_isr(void)
{
	u32_t signal_count;
	/*
	 * Signal the semaphore several times from an ISR.  After each signal,
	 * check the signal count.
	 */
	for (int i = 0; i < 5; i++) {
		sem_give_from_isr(&simple_sem);

		signal_count = k_sem_count_get(&simple_sem);
		zassert_true(signal_count == (i + 1),
			     "signal count missmatch Expected %d, got %d",
			     (i + 1), signal_count);
	}
}

/**
 * @brief Test semaphore count when given by a thread
 * @ingroup kernel_semaphore_tests
 * @verify{@req{363}}
 */
void test_simple_sem_from_thread(void)
{
	u32_t signal_count;
	/*
	 * Signal the semaphore several times from a task.  After each signal,
	 * check the signal count.
	 */
	k_sem_reset(&simple_sem);

	for (int i = 0; i < 5; i++) {
		k_sem_give(&simple_sem);
		signal_count = k_sem_count_get(&simple_sem);
		zassert_true(signal_count == (i + 1),
			     "signal count missmatch Expected %d, got %d",
			     (i + 1), signal_count);
	}
}

/* ztest main entry*/
void test_main(void)
{
	k_thread_access_grant(k_current_get(),
			      &simple_sem);

	ztest_test_suite(test_semaphore,
			ztest_unit_test(test_simple_sem_from_isr),
			ztest_unit_test(test_simple_sem_from_thread));
	ztest_run_test_suite(test_semaphore);
}
/******************************************************************************/
