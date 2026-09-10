/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  A nested verb (SHOW STATISTICS) must be looked up in its parent's
**	sub-verb list, not in the parent table. __cli$parse_verb() descends into
**	vsel->next. The sub-verb follows the parent verb immediately - no qualifier
**	stands between them - so argv[1] is the sub-verb; qualifiers after it are
**	handled by the sub-verb's own parameter/qualifier parse.
**
**  MODULE:  test_cli_nested.c
**--
*/
#define __MODULE__ "NEST$"

#include "test_harness.h"
#include "utility_routines.h"
#include "cli_routines.h"

static int s_ran = 0;					/* 1 = statistics, 2 = version	*/

static int s_act_stat (CLI_CTX *a_c, void *a_a) { (void) a_c; (void) a_a; s_ran = 1; return STS$K_SUCCESS; }
static int s_act_ver  (CLI_CTX *a_c, void *a_a) { (void) a_c; (void) a_a; s_ran = 2; return STS$K_SUCCESS; }

static CLI_PQDESC s_stat_q [] = {
	{ .name={$ASCINI("csv")}, .type=CLI$K_OPT, .pn=CLI$K_QUAL },
	{ .name={0} }
};
static CLI_VERB s_show_subs [] = {
	{ .name={$ASCINI("statistics")}, .act_rtn=s_act_stat, .quals=s_stat_q },
	{ .name={$ASCINI("version")},    .act_rtn=s_act_ver  },
	{ .name={0} }
};
static CLI_VERB s_cmds [] = {
	{ .name={$ASCINI("show")}, .next=s_show_subs },
	{ .name={0} }
};

int main (void)
{
	void	*l_ctx;
	int	l_st;

	printf("== cli nested-verb regression ==\n");

	/* The sub-verb is found in the sub-verb list and its action dispatched. */
	{
	char *l_a[] = { "show", "statistics" };
	s_ran = 0;
	l_st = __cli$parse(s_cmds, 0, 2, l_a, &l_ctx);
	CHECK( 1 & l_st, "'show statistics' must parse");
	if ( 1 & l_st )
		__cli$dispatch(l_ctx);
	CHECK( s_ran == 1, "'show statistics' must reach the statistics sub-verb");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "show", "version" };
	s_ran = 0;
	l_st = __cli$parse(s_cmds, 0, 2, l_a, &l_ctx);
	CHECK( 1 & l_st, "'show version' must parse");
	if ( 1 & l_st )
		__cli$dispatch(l_ctx);
	CHECK( s_ran == 2, "'show version' must reach the version sub-verb");
	__cli$cleanup(l_ctx);
	}

	/* A qualifier AFTER the sub-verb is the sub-verb's own, and is accepted. */
	{
	char *l_a[] = { "show", "statistics", "--csv" };
	unsigned l_state;
	s_ran = 0;
	l_st = __cli$parse(s_cmds, 0, 3, l_a, &l_ctx);
	CHECK( 1 & l_st, "'show statistics --csv' must parse");
	if ( 1 & l_st )
		__cli$dispatch(l_ctx);
	CHECK( s_ran == 1, "'show statistics --csv' must still reach statistics");
	CHECK( 1 & __cli$present(l_ctx, &s_stat_q[0], &l_state), "the sub-verb's /csv must be present");
	__cli$cleanup(l_ctx);
	}

	/* An unknown sub-verb is rejected, not mistaken for the parent. */
	{
	char *l_a[] = { "show", "nonesuch" };
	CHECK( !(1 & __cli$parse(s_cmds, 0, 2, l_a, &l_ctx)), "an unknown sub-verb must be rejected");
	__cli$cleanup(l_ctx);
	}

	TEST_DONE();
}
