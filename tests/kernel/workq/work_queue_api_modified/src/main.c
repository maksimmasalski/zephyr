/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Workqueue Requirements Tests
 * @defgroup kernel_workqueue_modified_tests Workqueue
 * @ingroup all_tests
 * @{
 * @}
 */

#include <zephyr.h>
#include <ztest.h>
#include <stdio.h>
#include <inttypes.h>

#define TIMEOUT 100
#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#define MY_PRIORITY 5
#define NUM_OF_WORK 3
#define SYNC_SEM_INIT_VAL (0U)
#define COM_SEM_MAX_VAL (1U)
#define COM_SEM_INIT_VAL (0U)
#define MAX_WORK_Q_NUMBER 10

struct k_work_q work_q_max_number[MAX_WORK_Q_NUMBER];
struct k_work_q work_q_1;
struct k_work work_item, work_item_1, work_item_2, work_item_3;
struct k_delayed_work work_item_delayed;
struct k_sem common_sema, sync_sema, sema_fifo_one, sema_fifo_two;
K_THREAD_STACK_DEFINE(my_stack_area, STACK_SIZE);
K_THREAD_STACK_DEFINE(new_stack_area[MAX_WORK_Q_NUMBER], STACK_SIZE);


/* Common function using like a handler for workqueue tests */
void new_common_work_handler(struct k_work *unused)
{
	k_sem_give(&sync_sema);
}

/*
 * @brief Test work item can be supplied with a user defined
 * call back function
 *
 * @details
 * Creating a work item, then add handler function
 * to that work item. Then process that work item.
 * To check that handler function executed successfully,
 * we use semaphore sync_sema with initial count 0.
 * Handler function gives semaphore, then we wait for that semaphore
 * from the test function body.
 * If semaphore was obtained successfully, test passed.
 *
 * @ingroup kernel workqueue tests
 * @verify{@req{026}}
 * @verify{@req{029}}
 */
void test_work_item_supplied_with_func(void)
{
	u32_t sem_count = 0;

	k_sem_reset(&sync_sema);

	/* TESTPOINT: init work item with a user-defined function */
	k_work_init(&work_item, new_common_work_handler);
	k_work_submit_to_queue(&work_q_1, &work_item);

	k_sem_take(&sync_sema, K_FOREVER);
	sem_count = k_sem_count_get(&sync_sema);
	zassert_equal(sem_count, COM_SEM_INIT_VAL, NULL);
}

/* Two handler functions fifo_work_first() and fifo_work_second */
/* are made for two work items to test first in, first out. */
/* It tests workqueue thread process work items */
/* in first in, first out manner */

void fifo_work_first(struct k_work *unused)
{
	k_sem_take(&sema_fifo_one, K_FOREVER);
	k_sem_give(&sema_fifo_two);
}

void fifo_work_second(struct k_work *unused)
{
	k_sem_take(&sema_fifo_two, K_FOREVER);
}

/*
 * @brief Test kernel process work items in fifo way
 *
 * @details
 * To test it we use 2 functions-handlers.
 * fifo_work_first() added to the work_item_1 and fifo_work_second()
 * added to the work_item_2.
 * We expect that firstly should run work_item_1, and only then
 * will run work_item_2.
 * To test it, we initialize semaphore sema_fifo_one
 * with count 1(available) and fifo_work_first() takes that semaphore.
 * fifo_work_second() is waiting for the semaphore sema_fifo_two,
 * which will be given from function fifo_work_first().
 * If work_item_2() will try to be executed before work_item_1(),
 * will happen a timeout error.
 * Because sema_fifo_two will be never obtained by fifo_work_second()
 * due to K_FOREVER macro in it while waiting for the semaphore.
 *
 * @ingroup kernel workqueue tests
 * @verify{@req{025}}
 */
void test_process_work_items_fifo(void)
{
	k_work_init(&work_item_1, fifo_work_first);
	k_work_init(&work_item_2, fifo_work_second);

	/* TESTPOINT: submit work items to the queue in fifo manner */
	k_work_submit_to_queue(&work_q_1, &work_item_1);
	k_work_submit_to_queue(&work_q_1, &work_item_2);
}

/*
 * @brief Test kernel support scheduling work item
 * that is to be processed only after specific period of time
 *
 * @details
 * For that test is using semaphore sync_sema, with initial count 0.
 * In that test we measure actual spent time and compare it with time
 * which was measured by function k_delayed_work_remaining_get().
 * Using function k_cycle_get_32() we measure actual spent time
 * in the period between delayed work submitted and delayed work
 * executed. To know that delayed work was executed, we use semaphore.
 * Right after semaphore was given from handler function, we stop
 * measuring actual time. Then compare results.
 * Please understand it is impossible to have 100% time accuracy,
 * due to different timer implementations on different architectures.
 * That test will fail for qemu_x86_64 or qemu_x86.
 *
 * @ingroup kernel workqueue tests
 * @verify{@req{027}}
 */
void test_sched_delayed_work_item(void)
{
	k_sem_reset(&sync_sema);
	s32_t ms_remain, ms_spent, start_time, stop_time, cycles_spent;
	s32_t ms_delta = 10;
	/* TESTPOINT: init delayed work to be processed */
	/* only after specific period of time */
	k_delayed_work_init(&work_item_delayed, new_common_work_handler);
	start_time = k_cycle_get_32();
	k_delayed_work_submit_to_queue(&work_q_1, &work_item_delayed, TIMEOUT);
	ms_remain = k_delayed_work_remaining_get(&work_item_delayed);

	k_sem_take(&sync_sema, K_FOREVER);
	stop_time = k_cycle_get_32();
	cycles_spent = stop_time - start_time;
	ms_spent = (u32_t)k_cyc_to_ms_floor32(cycles_spent);

	zassert_within(ms_spent, ms_remain, ms_delta, NULL);
}

/*
 * @brief Test app can be able to define any number of workqueues
 *
 * @details
 * We can define any number of workqueus using a variable of type
 * struct k_work_q. But it is useless, because we can't initialize
 * all that workqueus defined before.
 * More correct is to create a test that defines and initializes
 * maximum possible real number of the workqueues available according
 * to the stack size. That test defines and initializes maximum
 * number of the workqueues and starts them.
 *
 * @ingroup kernel workqueue tests
 * @verify{@req{030}}
 */
void test_workqueue_max_number(void)
{
	u32_t work_q_num = 0;

	for (u32_t i = 0; i < MAX_WORK_Q_NUMBER; i++) {
		work_q_num++;
		k_work_q_start(&work_q_max_number[i], new_stack_area[i],
			       K_THREAD_STACK_SIZEOF(new_stack_area[i]),
						MY_PRIORITY);
	}
	zassert_true(work_q_num == MAX_WORK_Q_NUMBER,
		     "Max number of the defined work queues not reached, "
		     "real number of the created work queues is "
		     "%d, expected %d", work_q_num, MAX_WORK_Q_NUMBER);
}

/**
 * @brief Test cancel already processed work item
 *
 * @details
 * That test is created to increase coverage and to check
 * that we can cancel already processed delayed work item.
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_cancel()
 */
void test_cancel_processed_work_item(void)
{
	int ret;

	k_sem_reset(&sync_sema);
	k_sem_reset(&sema_fifo_two);

	k_delayed_work_init(&work_item_delayed, new_common_work_handler);

	ret = k_delayed_work_cancel(&work_item_delayed);
	zassert_true(ret == -EINVAL, NULL);
	k_delayed_work_submit_to_queue(&work_q_1, &work_item_delayed, TIMEOUT);
	k_sem_take(&sync_sema, K_FOREVER);
	k_sem_give(&sema_fifo_two);

	/* TESTPOINT: try to delay already processed work item */
	ret = k_delayed_work_cancel(&work_item_delayed);

	k_sleep(100);
}

void test_main(void)
{
	k_work_q_start(&work_q_1, my_stack_area,
			K_THREAD_STACK_SIZEOF(my_stack_area), MY_PRIORITY);
	k_sem_init(&sync_sema, SYNC_SEM_INIT_VAL, NUM_OF_WORK);
			k_sem_init(&sema_fifo_one, COM_SEM_MAX_VAL,
			COM_SEM_MAX_VAL);
	k_sem_init(&sema_fifo_two, COM_SEM_INIT_VAL, COM_SEM_MAX_VAL);

	ztest_test_suite(workqueue_api_modified,
			ztest_unit_test(test_work_item_supplied_with_func),
			ztest_unit_test(test_process_work_items_fifo),
			ztest_unit_test(test_sched_delayed_work_item),
			ztest_unit_test(test_workqueue_max_number),
			ztest_unit_test(test_cancel_processed_work_item));
	ztest_run_test_suite(workqueue_api_modified);
}
