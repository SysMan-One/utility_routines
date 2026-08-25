/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  Exercises the cli_routines qualifier features: default values
**	(defval), negatable qualifiers (/NO<name>), value lists (/Q=(a,b,c)),
**	presence testing (__cli$present) and required qualifiers.  Guards
**	against a regression in any of them.
**
**  MODULE:  test_cli_features.c
**--
*/
#define __MODULE__ "CLIT$"

#include <string.h>
#include "test_harness.h"
#include "utility_routines.h"
#include "cli_routines.h"

static CLI_PQDESC s_quals [] = {
	{ .name={$ASCINI("out")},     .type=CLI$K_FILE,    .pn=CLI$K_QUAL, .defval={$ASCINI("stdout")} },
	{ .name={$ASCINI("verbose")}, .type=CLI$K_OPT,     .pn=CLI$K_QUAL, .flag=CLI$M_NEGATABLE },
	{ .name={$ASCINI("tags")},    .type=CLI$K_QSTRING, .pn=CLI$K_QUAL, .flag=CLI$M_LIST },
	{ .name={$ASCINI("key")},     .type=CLI$K_QSTRING, .pn=CLI$K_QUAL, .flag=CLI$M_REQUIRED },
	{ .name={0} }
};
static CLI_VERB s_cmds [] = {
	{ .name={$ASCINI("run")}, .quals=s_quals },
	{ .name={0} }
};

static int asc_eq (ASC *a_asc, const char *a_str)
{
	return	(a_asc->len == strlen(a_str)) && (0 == memcmp(a_asc->sts, a_str, a_asc->len));
}

int main (void)
{
	void		*l_ctx;
	ASC		l_v;
	unsigned	l_state;

	printf("== cli feature regression ==\n");

	/* defval - /out is absent, so its default is returned */
	{
	char *l_argv[] = { "run", "/key=K" };
	CHECK( 1 & __cli$parse(s_cmds, 0, 2, l_argv, &l_ctx), "parse (defaults) must succeed");
	CHECK( (1 & __cli$get_value(l_ctx, &s_quals[0], &l_v)) && asc_eq(&l_v, "stdout"),
		"defval: absent /out must return 'stdout'");
	__cli$cleanup(l_ctx);
	}

	/* an explicit value overrides the default */
	{
	char *l_argv[] = { "run", "/out=file.log", "/key=K" };
	__cli$parse(s_cmds, 0, 3, l_argv, &l_ctx);
	CHECK( (1 & __cli$get_value(l_ctx, &s_quals[0], &l_v)) && asc_eq(&l_v, "file.log"),
		"explicit /out must override defval");
	__cli$cleanup(l_ctx);
	}

	/* negatable - /noverbose is present and negated */
	{
	char *l_argv[] = { "run", "/noverbose", "/key=K" };
	__cli$parse(s_cmds, 0, 3, l_argv, &l_ctx);
	CHECK( (1 & __cli$present(l_ctx, &s_quals[1], &l_state)) && (l_state & CLI$M_NEGATED),
		"/noverbose must be present and CLI$M_NEGATED");
	__cli$cleanup(l_ctx);
	}
	/* affirmative - /verbose is present and not negated */
	{
	char *l_argv[] = { "run", "/verbose", "/key=K" };
	__cli$parse(s_cmds, 0, 3, l_argv, &l_ctx);
	CHECK( (1 & __cli$present(l_ctx, &s_quals[1], &l_state)) && !(l_state & CLI$M_NEGATED),
		"/verbose must be present and not negated");
	__cli$cleanup(l_ctx);
	}
	/* absent - present() reports absence */
	{
	char *l_argv[] = { "run", "/key=K" };
	__cli$parse(s_cmds, 0, 2, l_argv, &l_ctx);
	CHECK( !(1 & __cli$present(l_ctx, &s_quals[1], &l_state)),
		"absent /verbose must make present() warn");
	__cli$cleanup(l_ctx);
	}

	/* last wins - a repeated negatable qualifier reports its FINAL form (DCL) */
	{
	char *l_argv[] = { "run", "/verbose", "/noverbose", "/key=K" };
	__cli$parse(s_cmds, 0, 4, l_argv, &l_ctx);
	CHECK( (1 & __cli$present(l_ctx, &s_quals[1], &l_state)) && (l_state & CLI$M_NEGATED),
		"/verbose then /noverbose -> negated (last wins)");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_argv[] = { "run", "/noverbose", "/verbose", "/key=K" };
	__cli$parse(s_cmds, 0, 4, l_argv, &l_ctx);
	CHECK( (1 & __cli$present(l_ctx, &s_quals[1], &l_state)) && !(l_state & CLI$M_NEGATED),
		"/noverbose then /verbose -> affirmative (last wins)");
	__cli$cleanup(l_ctx);
	}

	/* list - successive get_value calls return each element, then stop */
	{
	char		*l_argv[] = { "run", "/tags=(alpha,beta,gamma)", "/key=K" };
	const char	*l_exp[] = { "alpha", "beta", "gamma" };
	int		l_n = 0;

	__cli$parse(s_cmds, 0, 3, l_argv, &l_ctx);
	while ( (1 & __cli$get_value(l_ctx, &s_quals[2], &l_v)) && (l_n < 10) )
		{
		CHECK( (l_n < 3) && asc_eq(&l_v, l_exp[l_n]), "list element %d must match", l_n);
		l_n++;
		}
	CHECK( l_n == 3, "list must yield exactly 3 elements (got %d)", l_n);
	__cli$cleanup(l_ctx);
	}

	/* required - missing /key fails the parse */
	{
	char *l_argv[] = { "run" };
	CHECK( !(1 & __cli$parse(s_cmds, 0, 1, l_argv, &l_ctx)),
		"missing required /key must fail the parse");
	__cli$cleanup(l_ctx);
	}
	/* required - /key present succeeds and round-trips */
	{
	char *l_argv[] = { "run", "/key=SECRET" };
	CHECK( 1 & __cli$parse(s_cmds, 0, 2, l_argv, &l_ctx), "required /key present must parse");
	CHECK( (1 & __cli$get_value(l_ctx, &s_quals[3], &l_v)) && asc_eq(&l_v, "SECRET"),
		"/key value must round-trip");
	__cli$cleanup(l_ctx);
	}

	TEST_DONE();
}
