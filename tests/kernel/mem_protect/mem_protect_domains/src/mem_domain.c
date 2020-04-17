/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <ztest.h>
#include <kernel_structs.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mempool.h>

#if defined(CONFIG_X86)
#define MEM_ALIGN 4096
#elif defined(CONFIG_ARM)
#define MEM_ALIGN 32
#elif defined(CONFIG_ARC)
#define MEM_ALIGN 8192
#else
#error "Test suite not compatible for the given architecture"
#endif

#define BUF0_SIZE 8192
#define BUF1_SIZE 8192

#define DESC_SIZE       sizeof(struct sys_mem_pool_block)
#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#define PRIORITY 5
#define BLK_SIZE_MIN 8
#define BLK_SIZE_MAX 16
#define BLK_NUM_MAX 4
#define BLK_ALIGN BLK_SIZE_MIN

extern bool expected_fault;
extern struct k_sem assert_sem;
struct k_sem sync_sem;

struct k_thread user_thread0, parent_thr, child_thr;
k_tid_t usr_tid0, usr_tid1, parent_tid, child_tid;
K_THREAD_STACK_DEFINE(user_thread0_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(child_thr_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(parent_thr_stack, STACK_SIZE);

struct k_mem_domain domain0, domain1, domain2, domain3, domain4, name_domain,
		    overlap_domain;

u8_t __aligned(MEM_ALIGN) buf0[MEM_ALIGN];
u8_t __aligned(MEM_ALIGN) buf_zero[0];
K_MEM_PARTITION_DEFINE(part0, buf0, sizeof(buf0),
                       K_MEM_PARTITION_P_RW_U_RW);
#if defined(CONFIG_X86)
K_MEM_PARTITION_DEFINE(wrong_part, 1, 0, K_MEM_PARTITION_P_RW_U_RW);
#elif defined(CONFIG_ARM)
K_MEM_PARTITION_DEFINE(wrong_part, 64, 32, K_MEM_PARTITION_P_RW_U_RW);
#elif defined(CONFIG_ARC)
K_MEM_PARTITION_DEFINE(wrong_part, 64, 32, K_MEM_PARTITION_P_RW_U_RW);
#else
#error "Test suite not compatible for the given architecture"
#endif


K_APPMEM_PARTITION_DEFINE(part1);
K_APP_BMEM(part1) u8_t __aligned(MEM_ALIGN) buf1[MEM_ALIGN];
struct k_mem_partition *app1_parts[] = {
	&part1
};

K_APPMEM_PARTITION_DEFINE(part_arch);
K_APP_BMEM(part_arch) u8_t __aligned(MEM_ALIGN) buf_arc[MEM_ALIGN];

K_APPMEM_PARTITION_DEFINE(part2);
K_APP_DMEM(part2) int part2_var = 1356;
K_APP_BMEM(part2) int part2_zeroed_var = 20420;
K_APP_BMEM(part2) int part2_bss_var;

SYS_MEM_POOL_DEFINE(data_pool, NULL, BLK_SIZE_MIN, BLK_SIZE_MAX,
		BLK_NUM_MAX, BLK_ALIGN, K_APP_DMEM_SECTION(part2));


/**
 * @brief Test access memory domain APIs allowed to supervisor threads only
 *
 * @details Create a test case to use a memory domain APIs k_mem_domain_init()
 * and k_mem_domain_add_partition()
 * Run that test in kernel mode -no errors should happen.
 * Run that test in user mode -Zephyr fatal error will happen (reason 0),
 * because user threads can't use memory domain APIs.
 * At the same time test that system support the definition of memory domains.
 *
 * @ingroup kernel_memprotect_tests
 *
 * @see k_mem_domain_init(), k_mem_domain_add_partition()
 *
 * @verify{@req{216}}
 * @verify{@req{228}}
 */
void test_mem_dom_api_kernel_thr_only(void)
{
	expected_fault = 0;
	k_mem_domain_init(&domain0, 0, NULL);
	k_mem_domain_add_partition(&domain0, &part0);
}

void user_handler_func(void *p1, void *p2, void *p3)
{
	/* Read the partition */
	u8_t read_data = buf1[0];

	/* Just to avoid compiler warning */
	(void) read_data;

	/* Writing to the partition, this should generate fault
	 * as the partition has read only permission for
	 * user threads
	 */
	buf1[0] = 10U;
	k_sem_give(&sync_sem);
}

/**
 * @brief Test system auto determine memory partition base addresses and sizes
 *
 * @details Ensure that system automatically determine memory partition
 * base address and size according to its content correctly by taking
 * memory partition's size and compare it with size of content. If size of
 * memory partition is equal to the size of content -test passes. If possible
 * to take base address of the memory partition -test passes.
 * Test that system supports the definition of memory partitions and it has
 * a starting address and a size.
 * Test that OS support removing thread from it is domain assignment.
 *
 * @ingroup kernel_memprotect_tests
 *
 * @verify{@req{217}}
 * @verify{@req{220}}
 * @verify{@req{221}}
 * @verify{@req{222}}
 */
void test_mem_part_auto_determ_size(void)
{
	k_sem_init(&sync_sem, 0, 1);
	k_thread_access_grant(&user_thread0, &sync_sem);
	zassert_true(part1.size == MEM_ALIGN, "Size of memory partition"
			" determined not correct according to its content");
	zassert_true(part1.start, "Base address of memory partition"
			" not determined at build time");

	k_mem_domain_init(&domain1, ARRAY_SIZE(app1_parts), app1_parts);
	k_mem_domain_remove_thread(k_current_get());
	usr_tid0 = k_thread_create(&user_thread0, user_thread0_stack,
					K_THREAD_STACK_SIZEOF(
						user_thread0_stack),
					user_handler_func,
					NULL, NULL, NULL,
					PRIORITY,
					K_USER, K_NO_WAIT);
	k_mem_domain_add_thread(&domain1, usr_tid0);
	k_sem_take(&sync_sem, K_FOREVER);
}

/**
 * @brief Test partitions sized per the constraints of the MMU hardware
 *
 * @details Test that system automatically determine memory partition size
 * according to the constraints of the platform's MMU hardware.
 * Different platforms like x86, ARM and ARC have different MMU hardware for
 * memory management. That test checks that MMU hardware works as expected
 * and gives for the memory partition the most suitable and possible size.
 *
 * @ingroup kernel_memprotect_tests
 *
 * @verify{@req{218}}
 */
void test_mem_part_auto_determ_size_per_mmu(void)
{
	zassert_true(part_arch.size == MEM_ALIGN, NULL);
}

/**
 * @brief Test assigning global data and BSS variables to memory partitions
 *
 * @details Test that system supports application assigning global data and BSS
 * variables using macros K_APP_BMEM() and K_APP_DMEM
 *
 * @ingroup kernel_memprotect_tests
 *
 * @verify{@req{219}}
 */
void test_mem_part_assign_bss_vars_zero(void)
{
	u32_t read_data;
	/* The global variable part2_var will be inside the bounds of part2
	* and be initialized with 1356 at boot.
	*/
	read_data = part2_var;
	zassert_true(read_data == 1356, NULL);

	/* The global variable part2_zeroed_var will be inside the bounds of
	 * part2 and must be zeroed at boot size K_APP_BMEM() was used,
	 * indicating a BSS variable.
	*/
	read_data = part2_zeroed_var;
	zassert_true(read_data == 0, NULL);

	/* The global variable part2_var will be inside the bounds of
	 * part2 and must be zeroed at boot size K_APP_BMEM() was used,
	 * indicating a BSS variable.
	*/
	read_data = part2_bss_var;
	zassert_true(read_data == 0, NULL);
}

/**
 * @brief Test system assert when adding memory partitions more then possible
 *
 * @details Add memory partitions one by one more then architecture allows
 * to add.
 * When partitions added more then it is allowed by architecture, test that
 * system assert for that case works correctly.
 *
 * @ingroup kernel_memprotect_tests
 *
 * @verify{@req{225}}
 */
void test_mem_part_assert_add_overmax(void)
{
	u8_t max_partitions;
	k_mem_domain_init(&domain2, 0, NULL);
	max_partitions = arch_mem_domain_max_partitions_get();
	for (int i=0; i<=max_partitions; i++) {
		k_mem_domain_add_partition(&domain2, &part0);
	}

}

/**
 * @brief Test system assert if not correct memory partition size and address
 *
 * @details Define memory partition where start address is 1 and size is 0
 * It will make assert, because summ of the (part->start+part->size) must be
 * more then (part->start), but in our case that condition will fail.
 * That way we check system assertion for that case.
 * That test works only in x86.
 * It can't test ASSERTION fail on ARM and ARC, on that boards that test will
 * just pass without any assertion.
 *
 * @ingroup kernel_memprotect_tests
 *
 * @verify{@req{223}}
 */
void test_mem_part_assert_data_correct(void)
{
	k_mem_domain_init(&domain3, 0, NULL);
	k_mem_domain_add_partition(&domain3, &wrong_part);
}

void child_thr_handler(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
}

void parent_thr_handler(void *p1, void *p2, void *p3)
{
	k_tid_t child_tid = k_thread_create(&child_thr, child_thr_stack,
					K_THREAD_STACK_SIZEOF(child_thr_stack),
					child_thr_handler,
					NULL, NULL, NULL,
					PRIORITY, 0, K_NO_WAIT);
	struct k_thread *thread = child_tid;
	zassert_true(thread->mem_domain_info.mem_domain == &domain4, NULL);
	k_sem_give(&sync_sem);
}

/**
 * @brief Test memory partition is inherited by the child thread
 *
 * @details Define memory partition and add it to the parent thread.
 * Parent thread creates a child thread. That child thread should be already
 * added to the parent's memory domain too.
 * It will be checked by using thread->mem_domain_info.mem_domain
 * If that child thread structure is equal to the current
 * memory domain address, then pass the test. It means child thread inherited
 * the memory domain assignment of their parent.
 *
 * @ingroup kernel_memprotect_tests
 *
 * @see k_mem_add_thread(), k_mem_remove_thread()
 *
 * @verify{@req{229}}
 */
void test_mem_part_inherit_by_child_thr(void)
{
	k_sem_reset(&sync_sem);
	k_mem_domain_init(&domain4, 0, NULL);
	k_mem_domain_add_partition(&domain4, &part0);
	k_mem_domain_remove_thread(k_current_get());
	k_tid_t parent_tid = k_thread_create(&parent_thr, parent_thr_stack,
					K_THREAD_STACK_SIZEOF(parent_thr_stack),
					parent_thr_handler,
					NULL, NULL, NULL,
					PRIORITY, 0, K_NO_WAIT);
	k_mem_domain_add_thread(&domain4, parent_tid);
	k_sem_take(&sync_sem, K_FOREVER);
}

/**
 * @brief Test system provide means to obtain names of the data and BSS sections
 *
 * @details Define memory partition and define system memory pool using macros
 * SYS_MEM_POOL_DEFINE, section name of the destination binary section for pool
 * data will be obtained at build time by macros K_APP_DMEM_SECTION() which
 * obtaines a section name. Then to check that system memory pool initialized
 * correctly allocate a block from it and check that it is not NULL.
 *
 * @ingroup kernel_memprotect_tests
 *
 * @see K_APP_DMEM_SECTION()
 *
 * @verify{@req{227}}
 */
void test_macros_obtain_names_data_bss(void)
{
	sys_mem_pool_init(&data_pool);
	void *block;

	block = sys_mem_pool_alloc(&data_pool, BLK_SIZE_MAX - DESC_SIZE);
	zassert_not_null(block, NULL);
}

/**
 * @brief Test system assert when add a partition to a memory domain,
 * that it does not overlap with any other partitions in the domain
 *
 * @details Github issue 24859
 *
 * @ingroup kernel_memprotect_tests
 *
 * @see
 *
 * @verify{@req{231}}
 */
void test_mem_part_add_not_overlap(void)
{
	/* NOT READY AT ALL */
	u8_t max_partitions;
	k_mem_domain_init(&overlap_domain, 0, NULL);
	k_mem_domain_add_partition(&overlap_domain, &part0);
	max_partitions = arch_mem_domain_max_partitions_get();
}
void test_mem_part_support_subsys_lib(void)
{
//Github issue 24854
}
