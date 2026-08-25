/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  A tiny check harness shared by the regression tests.  CHECK()
**	records a pass/fail and prints the message on failure; TEST_DONE()
**	prints the tally and returns a process exit code (non-zero on any
**	failure), so a test drops straight into a build pipeline.
**
**  MODULE:  test_harness.h
**--
*/
#ifndef	__TEST_HARNESS_H__
#define	__TEST_HARNESS_H__	1

#include <stdio.h>

static int g_total = 0, g_fail = 0;

#define	CHECK(cond, ...)	do {			\
		g_total++;				\
		if ( !(cond) )				\
			{				\
			g_fail++;			\
			printf("  FAIL: ");		\
			printf(__VA_ARGS__);		\
			printf("\n");			\
			}				\
	} while (0)

#define	TEST_DONE()	do {						\
		printf("== %d checks, %d failed ==\n", g_total, g_fail);	\
		return	g_fail ? 1 : 0;					\
	} while (0)

#endif	/* __TEST_HARNESS_H__ */
