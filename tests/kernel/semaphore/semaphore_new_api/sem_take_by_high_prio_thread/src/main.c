/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

/* Macro declarations */
#define SEM_INIT_COUNT (1U)
#define SEM_MAX_COUNT  (1U)

/* size of stack area used by each thread */
#define STACKSIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)

#define THREAD_1_PRIO 2
#define THREAD_2_PRIO -1
#define THREAD_3_PRIO -1
#define THREAD_4_PRIO 2

/* Kobject declaration */
K_SEM_DEFINE(simple_sem, SEM_INIT_COUNT, SEM_MAX_COUNT);

K_THREAD_STACK_DEFINE(stack2, STACKSIZE);
static struct k_thread thread2;

K_THREAD_STACK_DEFINE(stack3, STACKSIZE);
static struct k_thread thread3;

K_THREAD_STACK_DEFINE(stack4, STACKSIZE);
static struct k_thread thread4;

u32_t thread_flag = 0;
u32_t thread1_id = 1;
u32_t thread2_id = 2;
u32_t thread3_id = 3;
u32_t thread4_id = 4;

/**
 * @brief Test when the semaphore is given, it is taken by the high prio thread that has waited longest
 * @ingroup kernel_semaphore_tests
 * @verify{@req{359}}
 */
static void thread4_low_prio_wait_shortest(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	printk("Thread 4 waiting for sem \n");
	k_sem_take(&simple_sem, K_FOREVER);
	printk("Thread 4 took semaphore\n");
	thread_flag = 4;
	zassert_true((thread_flag == thread2_id),
			"Wrong thread took semaphore, "
			"expected thread %d take semaphore, instead thread %d took it", thread2_id, thread_flag);
}

static void thread3_high_prio_wait_short(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	/* spawn thread 4 low prio wait short*/
	k_thread_create(&thread4, stack4,
			STACKSIZE, thread4_low_prio_wait_shortest, NULL, NULL, NULL,
			THREAD_4_PRIO, 0, K_NO_WAIT);
	printk("Thread 3 created thread 4\n");
	printk("Thread 3 waiting for sem \n");
	k_sem_take(&simple_sem, K_FOREVER);
	printk("Thread 3 took semaphore\n");
	thread_flag = 3;
	zassert_true((thread_flag == thread2_id),
			"Wrong thread took semaphore, "
			"expected thread %d take semaphore, instead thread %d took it", thread2_id, thread_flag);
}

static void thread2_high_prio_wait_long(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	/* spawn thread 3 high prio which will wait less than thread 2*/
	k_thread_create(&thread3, stack3,
			STACKSIZE, thread3_high_prio_wait_short, NULL, NULL, NULL,
			THREAD_3_PRIO, 0, K_NO_WAIT);
	printk("Thread 2 created thread 3\n");
	printk("Thread 2 waiting for sem \n");
	k_sem_take(&simple_sem, K_FOREVER);
	printk("Thread 2 with high priority and which waited longest took semaphore. Success.\n");
}

static void test_thread_high_prio(void)
{
	k_sem_take(&simple_sem, K_FOREVER);

	/*After spawn thread 1, now spawn high prio thread 2 which will wait longest */
	k_thread_create(&thread2, stack2,
			STACKSIZE, thread2_high_prio_wait_long, NULL, NULL, NULL,
			THREAD_2_PRIO, 0, K_NO_WAIT);

	printk("Thread 1 created thread 2 \n");
	k_sleep(100);
	k_sem_give(&simple_sem);
	printk("Thread 1 gave semaphore\n");
}

K_THREAD_DEFINE(thread1, STACKSIZE, test_thread_high_prio, NULL, NULL, NULL, 7, 0, K_NO_WAIT);

/* ztest main entry*/
void test_main(void)
{
	ztest_test_suite(test_sem_take_high_prio_thread,
			ztest_unit_test(test_thread_high_prio));
	ztest_run_test_suite(test_sem_take_high_prio_thread);
}
