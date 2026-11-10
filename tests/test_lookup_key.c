/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  __util$lookup_key(): exact and case-blind lookups, and the
**	ABBREV mode - a full keyword wins, a unique abbreviation selects,
**	synonyms (the same value) are not ambiguous, an ambiguous one is
**	STS$K_WARN; the entry handed back is always inside the table.
**
**  MODULE:  test_lookup_key.c
**--
*/
#define __MODULE__ "LKT$"

#include <string.h>
#include "test_harness.h"
#include "utility_routines.h"

enum { K_BOOT = 1, K_CONT, K_CRASH, K_SHOW, K_SET, K_SETUP };

static KWDENT s_tbl[] = {
	{ {$ASCINI("BOOT")},	{.val = K_BOOT} },
	{ {$ASCINI("B")},	{.val = K_BOOT} },
	{ {$ASCINI("CONTINUE")},{.val = K_CONT} },
	{ {$ASCINI("CRASH")},	{.val = K_CRASH} },
	{ {$ASCINI("SHOW")},	{.val = K_SHOW} },
	{ {$ASCINI("SH")},	{.val = K_SHOW} },
	{ {$ASCINI("SETUP")},	{.val = K_SETUP} },
	{ {$ASCINI("SET")},	{.val = K_SET} },
};
#define	N	((int) (sizeof(s_tbl) / sizeof(s_tbl[0])))

/* status, value (-1 if none), and the entry stays in the table */
static int s_look (const char *a_s, int a_flags, int *a_val)
{
KWDENT	*l_k = NULL;
int	l_sts = __util$lookup_key (a_s, (int) strlen (a_s), s_tbl, N, &l_k, a_flags);

	*a_val = -1;
	if ( l_k )
		{
		CHECK(l_k >= s_tbl && l_k < s_tbl + N, "'%s': entry inside the table", a_s);
		*a_val = l_k->val;
		}
	return l_sts;
}

int main (void)
{
int	v, sts;
const int nc = UTIL$K_LOOKUP_NCASE, ab = UTIL$K_LOOKUP_NCASE | UTIL$K_LOOKUP_ABBREV;

	/* exact, case-sensitive */
	sts = s_look("BOOT", 0, &v);	CHECK(sts == STS$K_SUCCESS && v == K_BOOT, "BOOT exact: %d %d", sts, v);
	sts = s_look("boot", 0, &v);	CHECK(sts == STS$K_ERROR, "boot exact case-sensitive must fail: %d", sts);
	sts = s_look("SET", 0, &v);	CHECK(sts == STS$K_SUCCESS && v == K_SET, "SET after the longer SETUP: %d %d", sts, v);
	sts = s_look("SETU", 0, &v);	CHECK(sts == STS$K_ERROR, "SETU exact must fail: %d", sts);

	/* exact, any case */
	sts = s_look("sEt", nc, &v);	CHECK(sts == STS$K_SUCCESS && v == K_SET, "sEt NCASE: %d %d", sts, v);
	sts = s_look("b", nc, &v);	CHECK(sts == STS$K_SUCCESS && v == K_BOOT, "b NCASE: %d %d", sts, v);
	sts = s_look("cont", nc, &v);	CHECK(sts == STS$K_ERROR, "cont without ABBREV must fail: %d", sts);
	sts = s_look("", nc, &v);	CHECK(sts == STS$K_ERROR, "empty must fail: %d", sts);
	sts = s_look("BOOTX", nc, &v);	CHECK(sts == STS$K_ERROR, "BOOTX must fail: %d", sts);

	/* abbreviations */
	sts = s_look("cont", ab, &v);	CHECK(sts == STS$K_SUCCESS && v == K_CONT, "cont: %d %d", sts, v);
	sts = s_look("cr", ab, &v);	CHECK(sts == STS$K_SUCCESS && v == K_CRASH, "cr: %d %d", sts, v);
	sts = s_look("c", ab, &v);	CHECK(sts == STS$K_WARN, "c: CONTINUE or CRASH - ambiguous: %d", sts);
	sts = s_look("sho", ab, &v);	CHECK(sts == STS$K_SUCCESS && v == K_SHOW, "sho: %d %d", sts, v);
	sts = s_look("s", ab, &v);	CHECK(sts == STS$K_WARN, "s: SHOW/SET/SETUP - ambiguous: %d", sts);
	sts = s_look("se", ab, &v);	CHECK(sts == STS$K_WARN, "se: SET or SETUP - ambiguous: %d", sts);
	sts = s_look("set", ab, &v);	CHECK(sts == STS$K_SUCCESS && v == K_SET, "set spelled out wins over SETUP: %d %d", sts, v);
	sts = s_look("setu", ab, &v);	CHECK(sts == STS$K_SUCCESS && v == K_SETUP, "setu: %d %d", sts, v);
	sts = s_look("bo", ab, &v);	CHECK(sts == STS$K_SUCCESS && v == K_BOOT, "bo: %d %d", sts, v);
	sts = s_look("b", ab, &v);	CHECK(sts == STS$K_SUCCESS && v == K_BOOT, "b full synonym: %d %d", sts, v);
	sts = s_look("showx", ab, &v);	CHECK(sts == STS$K_ERROR, "showx must fail: %d", sts);
	sts = s_look("x", ab, &v);	CHECK(sts == STS$K_ERROR, "x must fail: %d", sts);
	sts = s_look("Sho", UTIL$K_LOOKUP_ABBREV, &v); CHECK(sts == STS$K_ERROR, "Sho ABBREV case-sensitive must fail: %d", sts);

	TEST_DONE();
}
