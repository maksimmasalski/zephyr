/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <irq_offload.h>

#define SEM_INIT_VAL (0U)
#define SEM_MAX_VAL (10U)
#define sem_give_from_isr(sema) irq_offload(isr_sem_give, sema)
/* the max count of semaphores can be defined */
#define MAX_COUNT 39

#define SEM_TIMEOUT (K_MSEC(100))
#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)

K_SEM_DEFINE(simple_sem, SEM_INIT_VAL, SEM_MAX_VAL);
K_SEM_DEFINE(high_prio_long_sem, SEM_INIT_VAL, SEM_MAX_VAL);
K_SEM_DEFINE(high_prio_sem, SEM_INIT_VAL, SEM_MAX_VAL);
K_SEM_DEFINE(mid_prio_sem, SEM_INIT_VAL, SEM_MAX_VAL);
K_SEM_DEFINE(low_prio_sem, SEM_INIT_VAL, SEM_MAX_VAL);
K_SEM_DEFINE(common_sem, SEM_INIT_VAL, SEM_MAX_VAL);

K_THREAD_STACK_DEFINE(stack_1, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack_2, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack_3, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack_4, STACK_SIZE);

struct k_sem sem_1;
struct k_thread thread_1, thread_2, thread_3, thread_4;
u32_t counter;

/**
 * @brief Semaphore tests
 * @defgroup kernel_semaphore_tests Semaphore
 * @ingroup all_tests
 */

/**
 * @brief Test semaphore defined at compile time
 *
 * @details
 * - Get the semaphore count
 * - Verify whether the semaphore count equals to initialized
 *   value
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{361}}
 */
void test_k_sem_define(void)
{
	u32_t sem_count;

	/* Get the semaphore count */
	sem_count = k_sem_count_get(&simple_sem);
	/* Verify whether the semaphore count equals to initialized
	 * value */
	zassert_true(sem_count == SEM_INIT_VAL,
			"semaphore initialized failed at compile time, "
			"expected count %d, got %d",
			SEM_INIT_VAL, sem_count);
}

/**
 * @brief Test semaphore initialization at running time
 *
 * @details
 * - initialize a semaphore with valid count and max limit
 * - initialize a semaphore with invalid max limit
 * - initialize a semaphore with invalid count
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{364}}
 */

void test_k_sem_init(void)
{
	s32_t ret;

	/* initialize a semaphore with valid count and max limit */
	ret = k_sem_init(&sem_1, SEM_INIT_VAL, SEM_MAX_VAL);
	zassert_true(ret == 0, "k_sem_init failed");

	/* initialize a semaphore with invalid max limit */
	ret = k_sem_init(&sem_1, SEM_INIT_VAL, 0);
	zassert_true(ret == -EINVAL, "k_sem_init with invalid args");

	/* initialize a semaphore with invalid count */
	ret = k_sem_init(&sem_1, SEM_MAX_VAL + 1, SEM_MAX_VAL);
	zassert_true(ret == -EINVAL, "k_sem_init with invalid args");
}


/* Helper function:
 * give a semaphore right now */
void sem_give_task(void *p1, void *p2, void *p3)
{
	k_sem_give((struct k_sem *)p1);
}

/* Helper function:
 * give a semaphore with SEM_TIMEOUT MSEC delay */
void sem_give_task_with_delay(void *p1, void *p2, void *p3)
{
	k_sleep(K_MSEC(SEM_TIMEOUT));
	k_sem_give((struct k_sem *)p1);
}


/**
 * @brief Test the semaphore take operation with specified timeout
 *
 * @details
 * - Create a new thread, it will give semaphore
 * - Reset the semaphore's count to zero
 * - Take semaphore and wait it given by other threads
 *   in specified timeout
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{360}}
 * @verify{@req{362}}
 */
void test_k_sem_take_timeout(void)
{
	s32_t ret;
	u32_t sem_count;

	/* create a new thread, it will give semaphore */
	k_thread_create(&thread_1, stack_1, STACK_SIZE,
			sem_give_task, &sem_1, NULL, NULL,
			K_PRIO_PREEMPT(0), K_USER | K_INHERIT_PERMS,
			K_NO_WAIT);

	/* Reset the semaphore's count to zero */
	k_sem_reset(&sem_1);
	sem_count = k_sem_count_get(&sem_1);
	zassert_true(sem_count == 0U, "k_sme_reset failed");

	/* take semaphore and wait it given by other threads
	 * in specified timeout */
	ret = k_sem_take(&sem_1, SEM_TIMEOUT);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);
	k_thread_abort(&thread_1);
}


/**
 * @brief Test the semaphore take operation with an unavailable semaphore
 *
 * @details
 * - Reset the semaphore's count to zero, let it unavailable
 * - Take an unavailable semaphore and wait it until timeout
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{362}}
 */
void test_k_sem_take_timeout_fails(void)
{
	s32_t ret;
	u32_t sem_count;

	/* reset semaphore's count to zero, let it unavailable */
	k_sem_reset(&sem_1);
	sem_count = k_sem_count_get(&sem_1);
	zassert_true(sem_count == 0U, "k_sem_reset failed");

	/* take an unavailable semaphore and wait it until timeout */
	for (int i = 5; i >= 0; i--) {
		ret = k_sem_take(&sem_1, SEM_TIMEOUT);
		zassert_true(ret == -EAGAIN,
				"k_sem_take succeeded when it's not possible");
	}
}


/**
 * @brief Test the semaphore take operation with forever wait
 *
 * @details
 * - Create a new thread, it will give semaphore
 * - Reset the semaphore's count to zero
 * - Take semaphore and wait it given by other threads
 *   forever until it's available
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{360}}
 * @verify{@req{362}}
 */
void test_k_sem_take_forever(void)
{
	s32_t ret;
	u32_t sem_count;

	/* create a new thread, it will give semaphore */
	k_thread_create(&thread_1, stack_1, STACK_SIZE,
			sem_give_task_with_delay, &sem_1, NULL, NULL,
			K_PRIO_PREEMPT(0), K_USER | K_INHERIT_PERMS,
			K_NO_WAIT);

	/* reset the semaphore's count to zero */
	k_sem_reset(&sem_1);
	sem_count = k_sem_count_get(&sem_1);
	zassert_true(sem_count == 0U, "k_sem_reset failed");

	/* take semaphore and wait it given by other threads forever
	 * until it's available */
	ret = k_sem_take(&sem_1, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);
	k_thread_abort(&thread_1);
}

/* test the common semaphore taken by many threads simulaneously,
 * low priority thread sem take function */
void sem_take_multiple_low_prio_helper(void *p1, void *p2, void *p3)
{
	s32_t ret;

	ret = k_sem_take(&low_prio_sem, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);

	ret = k_sem_take(&common_sem, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);
	k_sem_give(&low_prio_sem);
}

void sem_take_multiple_mid_prio_helper(void *p1, void *p2, void *p3)
{
	s32_t ret;

	ret = k_sem_take(&mid_prio_sem, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);

	ret = k_sem_take(&common_sem, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);
	k_sem_give(&mid_prio_sem);
}

void sem_take_multiple_high_prio_helper(void *p1, void *p2, void *p3)
{
	s32_t ret;

	ret = k_sem_take(&high_prio_sem, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);

	ret = k_sem_take(&common_sem, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);
	k_sem_give(&high_prio_sem);
}

/* the highest priority and longest waiting time thread
 * sem take function */
void sem_take_multiple_high_prio_long_helper(void *p1, void *p2, void *p3)
{
	s32_t ret;

	ret = k_sem_take(&high_prio_long_sem, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);

	ret = k_sem_take(&common_sem, K_FOREVER);
	zassert_true(ret == 0, "k_sem_take failed with returned %d",
			ret);
	k_sem_give(&high_prio_long_sem);
}

/**
 * @brief Test semaphore take operation by multiple threads
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{358}}
 * @verify{@req{359}}
 */
void test_k_sem_take_by_multiple_threads(void)
{
	u32_t sem_count;

	k_sem_reset(&common_sem);
	sem_count = k_sem_count_get(&common_sem);
	zassert_true(sem_count == 0U, "k_sem_reset failed");

	/* creat 3 different priority thread waiting for the common_sem */
	k_thread_create(&thread_1, stack_1, STACK_SIZE,
			sem_take_multiple_low_prio_helper, NULL, NULL,
			NULL, K_PRIO_PREEMPT(3), K_USER | K_INHERIT_PERMS,
			K_NO_WAIT);
	k_thread_create(&thread_2, stack_2, STACK_SIZE,
			sem_take_multiple_mid_prio_helper, NULL, NULL,
			NULL, K_PRIO_PREEMPT(2), K_USER | K_INHERIT_PERMS,
			K_NO_WAIT);
	k_thread_create(&thread_3, stack_3, STACK_SIZE,
			sem_take_multiple_high_prio_long_helper, NULL, NULL,
			NULL, K_PRIO_PREEMPT(1), K_USER | K_INHERIT_PERMS,
			K_NO_WAIT);

	/* create another high priority thread, the same priority
	 * with thread_3 */
	k_thread_create(&thread_4, stack_4, STACK_SIZE,
			sem_take_multiple_high_prio_helper, NULL, NULL,
			NULL, K_PRIO_PREEMPT(1), K_USER | K_INHERIT_PERMS,
			K_NO_WAIT);

	/* wait 4 threads initialized */
	k_sleep(K_MSEC(20));

	/* make thread 1 to 3 waiting on common_sem */
	k_sem_give(&high_prio_long_sem);
	k_sem_give(&mid_prio_sem);
	k_sem_give(&low_prio_sem);

	/* delay 100ms to make thread_4 waiting on common_sem, then the waiting
	 * time of thread_4 is shorter than thread_3 */
	k_sleep(K_MSEC(100));
	k_sem_give(&high_prio_sem);

	k_sleep(K_MSEC(20));

	/* enable the high prio and long waiting thread(thread_3) to run */
	k_sem_give(&common_sem);
	k_sleep(K_MSEC(200));

	/* check the thread cpmpleted */
	sem_count = k_sem_count_get(&high_prio_long_sem);
	zassert_true(sem_count == 1U, "high priority and long waiting thread "
			"don't get the sem");
	sem_count = k_sem_count_get(&high_prio_sem);
	zassert_true(sem_count == 0U, "high priority thread shouldn't get the sem");
	sem_count = k_sem_count_get(&mid_prio_sem);
	zassert_true(sem_count == 0U, "mid priority thread shouldn't get the sem");
	sem_count = k_sem_count_get(&low_prio_sem);
	zassert_true(sem_count == 0U, "low priority thread shouldn't get the sem");

	/* enable the high prio thread(thread_4) to run */
	k_sem_give(&common_sem);
	k_sleep(K_MSEC(200));

	/* check the thread completed */
	sem_count = k_sem_count_get(&high_prio_long_sem);
	zassert_true(sem_count == 1U, "high priority and long waiting thread "
			"run again");
	sem_count = k_sem_count_get(&high_prio_sem);
	zassert_true(sem_count == 1U, "high priority thread don't get the sem");
	sem_count = k_sem_count_get(&mid_prio_sem);
	zassert_true(sem_count == 0U, "mid priority thread shouldn't get the sem");
	sem_count = k_sem_count_get(&low_prio_sem);
	zassert_true(sem_count == 0U, "low priority thread shouldn't get the sem");

	/* enable the mid prio thread(thread_2) to run */
	k_sem_give(&common_sem);
	k_sleep(K_MSEC(200));

	/* check the thread completed */
	sem_count = k_sem_count_get(&high_prio_long_sem);
	zassert_true(sem_count == 1U, "high priority and long waiting thread "
			"run again");
	sem_count = k_sem_count_get(&high_prio_sem);
	zassert_true(sem_count == 1U, "high priority thread run again");
	sem_count = k_sem_count_get(&mid_prio_sem);
	zassert_true(sem_count == 1U, "mid priority thread don't get the sem");
	sem_count = k_sem_count_get(&low_prio_sem);
	zassert_true(sem_count == 0U, "low priority thread shouldn't get the sem");

	/* enable the low prio thread(thread_1) to run */
	k_sem_give(&common_sem);
	k_sleep(K_MSEC(200));

	/* check the thread completed */
	sem_count = k_sem_count_get(&high_prio_long_sem);
	zassert_true(sem_count == 1U, "high priority and long waiting thread "
			"run again");
	sem_count = k_sem_count_get(&high_prio_sem);
	zassert_true(sem_count == 1U, "high priority thread run again");
	sem_count = k_sem_count_get(&mid_prio_sem);
	zassert_true(sem_count == 1U, "mid priority thread run again");
	sem_count = k_sem_count_get(&low_prio_sem);
	zassert_true(sem_count == 1U, "low priority thread don't get the sem");
}

/**
 * @brief Test the max value a semaphore can be given and taken
 *
 * @details
 * - Reset an initialized semaphore's count to zero
 * - Give the semaphore by a thread and verify the semaphore's
 *   count whether as expected
 * - Verify the max count a semaphore can reach
 * - Take the semaphore by a thread and verify the semaphore's
 *   count whether as expected
 * - Verify the max times a semaphore can be taken
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{356}}
 * @verify{@req{362}}
 * @verify{@req{364}}
 */
void test_k_sem_give_take(void)
{
	u32_t sem_count;
	s32_t ret;

	/* Reset an initialized semaphore's count to zero */
	k_sem_reset(&sem_1);
	sem_count = k_sem_count_get(&sem_1);
	zassert_true(sem_count == 0U, "k_sem_reset failed");

	/* Give the semaphore by a thread and verify the semaphore's
	 * count whether as expected */
	for (int i = 1; i <= SEM_MAX_VAL; i++) {
		k_sem_give(&sem_1);
		sem_count = k_sem_count_get(&sem_1);
		zassert_true(sem_count == i,
				"sem count mismatch expected %d, got %d",
				i, sem_count);
	}

	/* Verify the max count a semaphore can reach */
	/* continue to run k_sem_give, the count of sem_1 will not increase
	 * anymore */
	for (int i = 0; i < 5; i++) {
		k_sem_give(&sem_1);
		sem_count = k_sem_count_get(&sem_1);
		zassert_true(sem_count == SEM_MAX_VAL,
				"sem count mismatch expected %d, got %d",
				SEM_MAX_VAL, sem_count);
	}

	/* Take the semaphore by a thread and verify the semaphore's
	 * count whether as expected */
	for (int i = SEM_MAX_VAL - 1; i >= 0; i--) {
		ret = k_sem_take(&sem_1, K_NO_WAIT);
		zassert_true(ret == 0, "k_sem_take failed with returned %d",
				ret);

		sem_count = k_sem_count_get(&sem_1);
		zassert_true(sem_count == i,
				"sem count mismatch expected %d, got %d",
				i, sem_count);
	}

	/* Verify the max times a semaphore can be taken */
	/* continue to run k_sem_take, sem_1 can not be taken and it's count
	 * will be zero */
	for (int i = 0; i < 5; i++) {
		ret = k_sem_take(&sem_1, K_NO_WAIT);
		zassert_true(ret == -EBUSY, "k_sem_take failed with returned %d",
				ret);

		sem_count = k_sem_count_get(&sem_1);
		zassert_true(sem_count == 0U,
				"sem count mismatch expected %d, got %d",
				0, sem_count);
	}
}

/* Helper function:
 * give a semaphore in a interrupt context */
void isr_sem_give(void *sem)
{
	k_sem_give((struct k_sem *)sem);
}

/**
 * @brief Test whether a semaphore can be given by an ISR
 *
 * @details
 * - Reset an initialized semaphore's count to zero
 * - Create a loop, in each loop, do follow steps
 * - Give the semaphore from an ISR
 * - Get the semaphore's count
 * - Verify whether the semaphore's count as expected
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{363}}
 */
void test_k_sem_give_from_isr(void)
{
	u32_t sem_count;

	/* Reset an initialized semaphore's count to zero */
	k_sem_reset(&sem_1);

	/* Create a loop, in each loop, do follow steps */
	for (int i = 0; i < 5; i++) {
		/* Give the semaphore from an ISR */
		sem_give_from_isr(&sem_1);
		/* Get the semaphore count */
		sem_count = k_sem_count_get(&sem_1);
		/* Verify whether the semaphore's count as expected */
		zassert_true(sem_count == (i + 1),
				"sem_count missmatch expected %d, got %d",
				(i + 1), sem_count);
	}
}


/**
 * @brief Test whether a semaphore can be given by an thread
 *
 * @details
 * - Reset an initialized semaphore's count to zero
 * - Create a loop, in each loop, do follow steps
 * - Give the semaphore from a thread
 * - Get the semaphore's count
 * - Verify whether the semaphore's count as expected
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{363}}
 */
void test_k_sem_give_from_thread(void)
{
	u32_t sem_count;

	/* Reset an initialized semaphore's count to zero */
	k_sem_reset(&sem_1);

	/* Create a loop, in each loop, do follow steps */
	for (int i = 0; i < 5; i++) {
		/* Give the semaphore from a thread */
		k_sem_give(&sem_1);
		/* Get the semaphore count */
		sem_count = k_sem_count_get(&sem_1);
		/* Verify whether the semaphore's count as expected */
		zassert_true(sem_count == (i + 1),
				"sem_count missmatch expected %d, got %d",
				(i + 1), sem_count);
	}
}

/**
 * @brief Test the max number of semaphores can be defined
 *
 * @details
 * - Define a max count semaphore array and initialize them
 * - Verify the count of initialized semaphores whether equals
 *   to the max count
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{357}}
 */
void test_k_sem_max_number(void)
{
	u32_t sem_num = 0;

	/* define a max count semaphore array and initialize them */
	struct k_sem sem_array[MAX_COUNT];
	for (u32_t i = 0; i < MAX_COUNT; i++) {
		sem_num++;
		k_sem_init(&sem_array[i], SEM_INIT_VAL, SEM_MAX_VAL);
	}

	/* verify the count of initialized semaphores whether equals
	 * to the max count */
	zassert_true(sem_num == MAX_COUNT,
			"Max number of thre created semapohores not "
			"reached, rel numberof created semaphores is "
			"%d, expected %d", sem_num, MAX_COUNT);
}

/* Helper function:
 * mutual exclusion via semaphores*/
void sem_queue_mutual_exclusion(void *p1, void *p2, void *p3)
{
	u32_t sem_count;

	/* start of the critical section */
	k_sem_take(&sem_1, K_FOREVER);
	sem_count = k_sem_count_get(&sem_1);

	counter++;
	zassert_true(counter == 1, "Counter expected be 1, really is %d\n",
			counter);
	/* make current thread sleep to let second thread take semaphore */
	counter = 0;
	k_sem_give(&sem_1);
}

/**
 * @brief Test threads' mutual exclusion by semaphore
 *
 * @ingroup kernel_semaphore_tests
 * @verify{@req{355}}
 */
void test_sem_queue_mutual_exclusion(void)
{
	k_sem_reset(&sem_1);
	k_sem_give(&sem_1);

	k_thread_create(&thread_1, stack_1, STACK_SIZE,
			sem_queue_mutual_exclusion, NULL, NULL,
			NULL, K_PRIO_PREEMPT(-1), 0,
			K_NO_WAIT);

	sem_queue_mutual_exclusion(NULL, NULL, NULL);

	k_sleep(K_MSEC(200));

	k_thread_abort(&thread_1);
}


/* ztest main entry*/
void test_main(void)
{
	/* give objects' access to current thread */
	k_thread_access_grant(k_current_get(), &simple_sem,
			&sem_1, &high_prio_long_sem, &high_prio_sem,
			&low_prio_sem, &mid_prio_sem, &stack_1, &stack_2, &stack_3,
			&stack_4, &thread_1, &thread_2, &thread_3, &thread_4,
			&common_sem);

	ztest_test_suite(test_semaphore_api,
			ztest_user_unit_test(test_k_sem_define),
			ztest_user_unit_test(test_k_sem_init),
			ztest_unit_test(test_k_sem_max_number),
			ztest_user_unit_test(test_k_sem_give_from_thread),
			ztest_unit_test(test_k_sem_give_from_isr),
			ztest_user_unit_test(test_k_sem_give_take),
			ztest_user_unit_test(test_k_sem_take_timeout),
			ztest_user_unit_test(test_k_sem_take_timeout_fails),
			ztest_user_unit_test(test_k_sem_take_forever),
			ztest_unit_test(test_sem_queue_mutual_exclusion),
			ztest_user_unit_test(test_k_sem_take_by_multiple_threads)
			);
	ztest_run_test_suite(test_semaphore_api);
}
/******************************************************************************/
