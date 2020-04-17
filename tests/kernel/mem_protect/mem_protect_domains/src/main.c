/*
 * Parts derived from tests/kernel/fatal/src/main.c, which has the
 * following copyright and license:
 *
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <ztest.h>
#include <kernel_structs.h>
#include <string.h>
#include <stdlib.h>

extern void test_mem_dom_api_kernel_thr_only(void);
extern void test_mem_part_auto_determ_size(void);
extern void test_mem_part_auto_determ_size_per_mmu(void);
extern void test_mem_part_assign_bss_vars_zero(void);
extern void test_mem_part_assert_add_overmax(void);
extern void test_mem_part_assert_data_correct(void);
extern void test_mem_part_inherit_by_child_thr(void);
extern void test_macros_obtain_names_data_bss(void);
extern void test_mem_part_add_not_overlap(void);


void test_main(void)
{
	k_thread_priority_set(k_current_get(), -1);

	ztest_test_suite(memory_protection_test_suite,
			ztest_unit_test(test_mem_dom_api_kernel_thr_only),
			ztest_user_unit_test(test_mem_dom_api_kernel_thr_only),
			ztest_unit_test(test_mem_part_auto_determ_size),
			ztest_unit_test(test_mem_part_auto_determ_size_per_mmu),
			ztest_unit_test(test_mem_part_inherit_by_child_thr),
			ztest_unit_test(test_macros_obtain_names_data_bss),
			ztest_unit_test(test_mem_part_add_not_overlap),
			ztest_unit_test(test_mem_part_assert_data_correct),
			ztest_unit_test(test_mem_part_assert_add_overmax)
			);
	ztest_run_test_suite(memory_protection_test_suite);
}
