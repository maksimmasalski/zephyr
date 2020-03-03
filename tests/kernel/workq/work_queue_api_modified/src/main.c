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

#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#define MY_PRIORITY 5
#define NUM_OF_WORK 3
#define TIMEOUT_TAKE_SEM 500
#define SYNC_SEM_INIT_VAL (0U)
#define COM_SEM_MAX_VAL (1U)
#define COM_SEM_INIT_VAL (0U)
#define MAX_WORK_Q_NUMBER 10

struct k_work_q work_q_max_number[MAX_WORK_Q_NUMBER];
struct k_work_q work_q_1, work_q_2;
struct k_work work_item, work_item_1, work_item_2, work_item_3;
struct k_delayed_work work_item_delayed;
struct k_sem common_sema, sync_sema, sema_one, sema_two;
K_THREAD_STACK_DEFINE(my_stack_area, STACK_SIZE);
K_THREAD_STACK_DEFINE(new_stack_area[MAX_WORK_Q_NUMBER], STACK_SIZE);
/* common variable to use during the tests */
u32_t sem_count = 0;
u32_t start_time;
u32_t stop_time;
u32_t cycles_spent;
u32_t ms_spent;

/**
 * @brief Common function using like a handler for workqueue tests
 * API call in it means successfull execution of that function
 *
 * @param unused of type k_work to make handler function accepted
 * by k_work_init
 *
 * @return N/A
 */
void common_work_handler(struct k_work *unused)
{
    k_sem_give(&sync_sema);
}

/*
 * @brief Test work item can be supplied with a user defined
 * call back function
 * @ingroup kernel workqueue tests
 * @verify{@req{2.1.1}}
 * @verify{@req{2.1.4}}
 */
void test_work_item_supplied_with_func(void)
{
    /**TESTPOINT: init work item with a user-defined function*/
    k_work_init(&work_item, common_work_handler);
    k_work_submit_to_queue(&work_q_1, &work_item);

    k_sem_take(&sync_sema, K_FOREVER);
    sem_count = k_sem_count_get(&sync_sema);
    zassert_equal(sem_count, COM_SEM_INIT_VAL, NULL);
}
/* TEST 2.1.1 END*/


/* TEST 2.1 START*/
/* Initially workitems were added in sequence Func 1, Func 2, Func 3
 * That if-else check will let us know that all 3 functions run
 * one after each other in expected sequence:
 * Func 1, Func 2, Func 3.
 * It tests workqueue thread process work items
 * in first in, first out manner */

void fifo_work_first(struct k_work *unused)
{
    k_sem_take(&sema_one, K_FOREVER);
    k_sem_give(&sema_two);
}

void fifo_work_second(struct k_work *unused)
{
    k_sem_take(&sema_two, K_FOREVER);
}

/*
 * @brief Test kernel process work items in fifo way
 * @ingroup kernel workqueue tests
 * @verify{@req{2.1}}
 */
void test_process_work_items_fifo(void)
{
    k_work_init(&work_item_1, fifo_work_first);
    k_work_init(&work_item_2, fifo_work_second);
    /**TESTPOINT: submit work items to the queue in fifo manner*/
    k_work_submit_to_queue(&work_q_1, &work_item_1);
    k_work_submit_to_queue(&work_q_1, &work_item_2);
}

/*
 * @brief Test kernel support scheduling work item
 * @ingroup kernel workqueue tests
 * @verify{@req{2.1.2}}
 */
void test_sched_delayed_work_item(void)
{
    k_sem_reset(&sync_sema);
    s32_t delay_time = 100;
    s32_t time_remain;
    /**TESTPOINT: init delayed work to be processed
      only after specific period of time*/
    k_delayed_work_init(&work_item_delayed, common_work_handler);
    start_time = k_cycle_get_32();
    k_delayed_work_submit_to_queue(&work_q_1, &work_item_delayed, delay_time);

    time_remain = k_delayed_work_remaining_get(&work_item_delayed);
    printk("Time remain(ms) %d\n", time_remain); //debug printk

    k_sem_take(&sync_sema, K_FOREVER);
    stop_time = k_cycle_get_32();

    cycles_spent = stop_time - start_time;
    ms_spent = (u32_t)k_cyc_to_ms_floor32(cycles_spent);
    printk("Time spent(ms) %u\n", ms_spent); //debug printk
    zassert_equal(time_remain, ms_spent, NULL);
}

/*
 * @brief Test app can be able to define any number of workqueues
 * @ingroup kernel workqueue tests
 * @verify{@req{2.1.5}}
 */
void test_workqueue_max_number(void)
{
    u32_t work_q_num = 0;
    for (u32_t i = 0; i < MAX_WORK_Q_NUMBER; i++) {
        work_q_num++;
        k_work_q_start(&work_q_max_number[i], new_stack_area[i],
            K_THREAD_STACK_SIZEOF(new_stack_area[i]), MY_PRIORITY);
    }
    zassert_true(work_q_num == MAX_WORK_Q_NUMBER,
            "Max number of the defined work queues not reached, "
            "real number of the created work queues is "
            "%d, expected %d", work_q_num, MAX_WORK_Q_NUMBER);

}

void test_main(void)
{
    k_work_q_start(&work_q_1, my_stack_area,
            K_THREAD_STACK_SIZEOF(my_stack_area), MY_PRIORITY);
    k_sem_init(&sync_sema, SYNC_SEM_INIT_VAL, NUM_OF_WORK);
    k_sem_init(&sema_one, COM_SEM_MAX_VAL, COM_SEM_MAX_VAL);
    k_sem_init(&sema_two, COM_SEM_INIT_VAL, COM_SEM_MAX_VAL);

    ztest_test_suite(workqueue_api_modified,
            ztest_unit_test(test_work_item_supplied_with_func),
            ztest_unit_test(test_process_work_items_fifo),
            ztest_unit_test(test_sched_delayed_work_item),
            ztest_unit_test(test_workqueue_max_number));
    ztest_run_test_suite(workqueue_api_modified);
}
