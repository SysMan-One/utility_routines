/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  A file-typed positional parameter (P1..P8) must accept a path that
**	begins with a slash - /etc/passwd - even though a leading slash is also the
**	VMS qualifier prefix. In the positional model a parameter is assigned by its
**	place, not by its content, so the path lands in the parameter; a qualifier
**	that follows it is still parsed. The parameter comes first, the qualifier
**	after - that ordering is the contract this test pins.
**
**  MODULE:  test_cli_filepath.c
**--
*/
#define __MODULE__ "FPTH$"

#include <string.h>
#include "test_harness.h"
#include "utility_routines.h"
#include "cli_routines.h"

static CLI_PQDESC s_params [] = {
	{ .name={$ASCINI("file")}, .type=CLI$K_FILE, .pn=CLI$K_P1 },
	{ .name={0} }
};
static CLI_PQDESC s_quals [] = {
	{ .name={$ASCINI("csv")}, .type=CLI$K_OPT, .pn=CLI$K_QUAL },
	{ .name={0} }
};
static CLI_VERB s_cmds [] = {
	{ .name={$ASCINI("edit")}, .params=s_params, .quals=s_quals },
	{ .name={0} }
};

static int asc_eq (ASC *a_asc, const char *a_str)
{
	return	(a_asc->len == (int) strlen(a_str)) && (0 == memcmp(a_asc->sts, a_str, a_asc->len));
}

int main (void)
{
	void	*l_ctx;
	ASC	l_v;

	printf("== cli file-path regression ==\n");

	/* A bare absolute path in the parameter slot. */
	{
	char *l_a[] = { "edit", "/etc/passwd" };
	CHECK( 1 & __cli$parse(s_cmds, 0, 2, l_a, &l_ctx), "'edit /etc/passwd' must parse");
	CHECK( (1 & __cli$get_value(l_ctx, &s_params[0], &l_v)) && asc_eq(&l_v, "/etc/passwd"),
		"the file parameter must hold the '/'-leading path");
	__cli$cleanup(l_ctx);
	}

	/* The same path with a trailing qualifier - both must survive. */
	{
	char *l_a[] = { "edit", "/etc/passwd", "/csv" };
	unsigned l_state;
	CHECK( 1 & __cli$parse(s_cmds, 0, 3, l_a, &l_ctx), "'edit /etc/passwd /csv' must parse");
	CHECK( (1 & __cli$get_value(l_ctx, &s_params[0], &l_v)) && asc_eq(&l_v, "/etc/passwd"),
		"the path must survive alongside a qualifier");
	CHECK( 1 & __cli$present(l_ctx, &s_quals[0], &l_state),
		"the trailing /csv must still be seen as present");
	__cli$cleanup(l_ctx);
	}

	/* A relative path is unaffected. */
	{
	char *l_a[] = { "edit", "./rel/path.txt" };
	CHECK( 1 & __cli$parse(s_cmds, 0, 2, l_a, &l_ctx), "'edit ./rel/path.txt' must parse");
	CHECK( (1 & __cli$get_value(l_ctx, &s_params[0], &l_v)) && asc_eq(&l_v, "./rel/path.txt"),
		"a relative path must be taken verbatim");
	__cli$cleanup(l_ctx);
	}

	/* A defined qualifier is NOT swallowed by the omitted optional parameter: a
	** '/csv' names a qualifier, so it is not mistaken for the file's value.
	*/
	{
	char *l_a[] = { "edit", "/csv" };
	unsigned l_state;
	CHECK( 1 & __cli$parse(s_cmds, 0, 2, l_a, &l_ctx), "'edit /csv' must parse");
	CHECK( !(1 & __cli$get_value(l_ctx, &s_params[0], &l_v)),
		"the omitted file parameter must stay empty, not take /csv");
	CHECK( 1 & __cli$present(l_ctx, &s_quals[0], &l_state),
		"/csv must be recognised as the qualifier, not a parameter value");
	__cli$cleanup(l_ctx);
	}

	/* A surplus token that fills neither a parameter nor a qualifier is rejected,
	** not dropped in silence.
	*/
	{
	char *l_a[] = { "edit", "one", "two" };
	CHECK( !(1 & __cli$parse(s_cmds, 0, 3, l_a, &l_ctx)),
		"a surplus token (no parameter slot left) must be rejected");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "edit", "/csv", "stray" };
	CHECK( !(1 & __cli$parse(s_cmds, 0, 3, l_a, &l_ctx)),
		"a bare token after a qualifier must be rejected");
	__cli$cleanup(l_ctx);
	}

	TEST_DONE();
}
