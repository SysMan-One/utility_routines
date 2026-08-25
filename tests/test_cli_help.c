/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  Guards the built-in /HELP detector.  The leading '-' or '/' is
**	mandatory: /HELP, -h and --help trigger the usage generator, but a bare
**	"help" must be treated as an ordinary token - a caller's own HELP verb, or
**	the value of a positional parameter - and must reach the grammar.  Also
**	checks that __cli$parse() zeroes *clictx before its early returns, so the
**	documented __cli$cleanup() idiom is safe after a failed parse.
**
**  MODULE:  test_cli_help.c
**--
*/
#define __MODULE__ "CLIH$"

#include <stdio.h>
#include "test_harness.h"
#include "utility_routines.h"
#include "cli_routines.h"

static int s_help_ran = 0;

static int s_help_action (CLI_CTX *a_ctx, void *a_arg)
{
	(void) a_ctx; (void) a_arg;
	s_help_ran = 1;
	return	STS$K_SUCCESS;
}

static CLI_PQDESC s_copy_p [] = {
	{ .name={$ASCINI("file")}, .type=CLI$K_FILE, .pn=CLI$K_P1 },
	{ .name={0} }
};
static CLI_VERB s_cmds [] = {
	{ .name={$ASCINI("copy")}, .params=s_copy_p },
	{ .name={$ASCINI("help")}, .act_rtn=s_help_action },	/* a caller's own HELP verb	*/
	{ .name={0} }
};

int main (void)
{
	void	*l_ctx;

	printf("== cli help detection regression ==\n");

	/* Punctuated forms trigger the built-in generator (parse returns WARN,
	** before parameter validation).
	*/
	{ char *l_a[] = { "copy", "/HELP" };
	  CHECK( STS$K_WARN == __cli$parse(s_cmds, 0, 2, l_a, &l_ctx), "'copy /HELP' -> help (WARN)");
	  __cli$cleanup(l_ctx); }
	{ char *l_a[] = { "copy", "-h" };
	  CHECK( STS$K_WARN == __cli$parse(s_cmds, 0, 2, l_a, &l_ctx), "'copy -h' -> help (WARN)");
	  __cli$cleanup(l_ctx); }
	{ char *l_a[] = { "copy", "--help" };
	  CHECK( STS$K_WARN == __cli$parse(s_cmds, 0, 2, l_a, &l_ctx), "'copy --help' -> help (WARN)");
	  __cli$cleanup(l_ctx); }

	/* A bare "help" as a parameter value must NOT be swallowed - it parses as P1. */
	{ char *l_a[] = { "copy", "help" };
	  CHECK( 1 & __cli$parse(s_cmds, 0, 2, l_a, &l_ctx),
		"'copy help' must parse (help is the P1 value, not the built-in)");
	  __cli$cleanup(l_ctx); }

	/* A caller's verb named HELP must reach the table and dispatch its action. */
	{ char *l_a[] = { "help" };
	  s_help_ran = 0;
	  CHECK( 1 & __cli$parse(s_cmds, 0, 1, l_a, &l_ctx), "verb 'help' must reach the grammar");
	  __cli$dispatch(l_ctx);
	  CHECK( s_help_ran, "verb 'help' must run its action, not the built-in generator");
	  __cli$cleanup(l_ctx); }

	/* *clictx must be NULL after a failed parse, so __cli$cleanup() is safe. */
	{ char *l_a[] = { "x" };
	  l_ctx = (void *) 0x1;					/* poison	*/
	  CHECK( !(1 & __cli$parse(s_cmds, 0, 0, l_a, &l_ctx)), "empty argc must fail the parse");
	  CHECK( l_ctx == NULL, "*clictx must be NULL after a failed parse");
	  __cli$cleanup(l_ctx);
	}

	TEST_DONE();
}
