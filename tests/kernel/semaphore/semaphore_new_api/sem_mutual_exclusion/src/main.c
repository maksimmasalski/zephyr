/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

/* Macro declarations */
#define SEM_INIT_COUNT (1U)
#define SEM_MAX_COUNT  (1U)
#define PRIORITY -1
#define STACKSIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#define COUNTER_MAX_VAL 1

/* Kobject declaration */
K_SEM_DEFINE(simple_sem, SEM_INIT_COUNT, SEM_MAX_COUNT);

static u32_t counter = 0;

/**
 * @brief Kernel provide a counting semaphore for queuing and mutual exclusion
 * @ingroup kernel_semaphore_tests
 * @verify{@req{355}}
 */
void test_sem_queue_mutual_exclusion(void)
{
	printk("Init code part started\n");
	static u32_t sem_count;
	static u32_t thread_prio;
	k_tid_t thread_id;

	thread_id = k_current_get();
	thread_prio = k_thread_priority_get(k_current_get());
	printk("I'm thread %d in the entry code with priority %d\n", (int)thread_id, thread_prio);

	/* start of the critical section */
	k_sem_take(&simple_sem, K_FOREVER);
	printk("Now I'm in the critical section\n");
	sem_count = k_sem_count_get(&simple_sem);
	printk("Semaphore count in the critical section is %d\n", sem_count);
	counter ++;
	printk("Counter expected be %d, really is %d\n", COUNTER_MAX_VAL, counter);
	zassert_true((counter == COUNTER_MAX_VAL),
			"Two threads entered into critical section at the same time, "
			"counter value should be %d, got %d",
			 COUNTER_MAX_VAL, counter);
	/* make current thread sleep to let second thread take semaphore */
	printk("Left critical section\n\n");
	counter = 0;
	k_sem_give(&simple_sem);
}

/* ztest main entry*/
void test_main(void)
{
	ztest_test_suite(test_semaphore_queuing_and_mutual_exclusion,
			ztest_unit_test(test_sem_queue_mutual_exclusion));
	ztest_run_test_suite(test_semaphore_queuing_and_mutual_exclusion);
}
/******************************************************************************/
K_THREAD_DEFINE(thread1, STACKSIZE, test_sem_queue_mutual_exclusion, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);
