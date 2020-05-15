/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mem_protect.h"
#include <syscall_handler.h>

/* function prototypes */
static inline void dummy_start(struct k_timer *timer)
{
	ARG_UNUSED(timer);
}
static inline void dummy_end(struct k_timer *timer)
{
	ARG_UNUSED(timer);
}

/* Kernel objects */
K_THREAD_STACK_DEFINE(test_1_stack, INHERIT_STACK_SIZE);
K_THREAD_STACK_DEFINE(parent_thr_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(child_thr_stack, STACK_SIZE);

K_MEM_POOL_DEFINE(res_pool, BLK_SIZE_MIN, BLK_SIZE_MAX, BLK_NUM_MAX, BLK_ALIGN);

K_SEM_DEFINE(inherit_sem, SEMAPHORE_INIT_COUNT, SEMAPHORE_MAX_COUNT);
K_MUTEX_DEFINE(inherit_mutex);
K_TIMER_DEFINE(inherit_timer, dummy_start, dummy_end);
K_MSGQ_DEFINE(inherit_msgq, MSG_Q_SIZE, MSG_Q_MAX_NUM_MSGS, MSG_Q_ALIGN);
struct k_thread test_1_tid, parent_thread, child_thread;

u8_t MEM_DOMAIN_ALIGNMENT inherit_buf[MEM_REGION_ALLOC]; /* for mem domain */

K_MEM_PARTITION_DEFINE(inherit_memory_partition,
		       inherit_buf,
		       sizeof(inherit_buf),
		       K_MEM_PARTITION_P_RW_U_RW);

struct k_mem_partition *inherit_memory_partition_array[] = {
	&inherit_memory_partition,
	&ztest_mem_partition
};

struct k_mem_domain inherit_mem_domain;

/* generic function to do check the access permissions. */
void access_test(void)
{
	u32_t msg_q_data = 0xA5A5;

	/* check for all accesses  */
	k_sem_give(&inherit_sem);
	k_mutex_lock(&inherit_mutex, K_FOREVER);
	(void) k_timer_status_get(&inherit_timer);
	k_msgq_put(&inherit_msgq, (void *)&msg_q_data, K_NO_WAIT);
	k_mutex_unlock(&inherit_mutex);
	inherit_buf[10] = 0xA5;
}

void test_thread_1_for_user(void *p1, void *p2, void *p3)
{
	access_test();
	ztest_test_pass();
}

void test_thread_1_for_SU(void *p1, void *p2, void *p3)
{
	valid_fault = false;
	USERSPACE_BARRIER;

	access_test();

	/* Check if user mode inherit is working if control is passed from SU */
	k_thread_user_mode_enter(test_thread_1_for_user, NULL, NULL, NULL);
}

/**
 * @brief Test object permission inheritance
 *
 * @ingroup kernel_memprotect_tests
 *
 * @see k_mem_domain_init(), k_mem_domain_add_thread(),
 * k_thread_access_grant()
 */
void test_permission_inheritance(void *p1, void *p2, void *p3)
{
	k_mem_domain_init(&inherit_mem_domain,
			  ARRAY_SIZE(inherit_memory_partition_array),
			  inherit_memory_partition_array);

	k_mem_domain_remove_thread(k_current_get());
	k_mem_domain_add_thread(&inherit_mem_domain, k_current_get());

	k_thread_access_grant(k_current_get(),
			      &inherit_sem,
			      &inherit_mutex,
			      &inherit_timer,
			      &inherit_msgq, &test_1_stack);

	k_thread_create(&test_1_tid,
			test_1_stack,
			INHERIT_STACK_SIZE,
			test_thread_1_for_SU,
			NULL, NULL, NULL,
			0, K_INHERIT_PERMS, K_NO_WAIT);

	k_sem_take(&sync_sem, SYNC_SEM_TIMEOUT);
}

struct k_mem_pool *z_impl_ret_resource_pool_ptr(void)
{
    return _current->resource_pool;
}

static inline struct k_mem_pool *z_vrfy_ret_resource_pool_ptr(void)
{
    return z_impl_ret_resource_pool_ptr();
}
#include <syscalls/ret_resource_pool_ptr_mrsh.c>
struct k_mem_pool *child_res_pool_ptr;
struct k_mem_pool *parent_res_pool_ptr;

void child_handler(void *p1, void *p2, void *p3)
{
    child_res_pool_ptr = ret_resource_pool_ptr();
    k_sem_give(&sync_sem);
}

void parent_handler(void *p1, void *p2, void *p3)
{
    parent_res_pool_ptr = ret_resource_pool_ptr();
    k_thread_create(&child_thread, child_thr_stack,
            K_THREAD_STACK_SIZEOF(child_thr_stack),
            child_handler,
            NULL, NULL, NULL,
            PRIORITY, 0, K_NO_WAIT);
}

/**
 * @brief Test child thread inherits parent's thread resource pool
 *
 * @details - Create a resource pool res_pool for the parent thread.
 * - Then special system call ret_resource_pool_ptr() returns pointer
 *   to the resource pool of the current thread.
 * - Call it in the parent_handler() and in the child_handler()
 * - Then in the main test function test_inherit_resource_pool()
 *   compare returned addresses
 * - If the addresses are the same, it means that child thread inherited
 *   resource pool of the parent's thread -test passed.
 *
 * @ingroup kernel_memprotect_tests
 *
 * @see k_thread_resource_pool_assign()
 */
void test_inherit_resource_pool(void *p1, void *p2, void *p3)
{
    k_sem_reset(&sync_sem);
    k_thread_create(&parent_thread, parent_thr_stack,
            K_THREAD_STACK_SIZEOF(parent_thr_stack),
            parent_handler,
            NULL, NULL, NULL,
            PRIORITY, 0, K_NO_WAIT);
    k_thread_resource_pool_assign(&parent_thread, &res_pool);
    k_sem_take(&sync_sem, K_FOREVER);
    zassert_true(parent_res_pool_ptr==child_res_pool_ptr,
            "Resource pool of the parent thread not inherited,"\
            " by child thread");
}
