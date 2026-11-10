/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  The parsing rules of cli_routines: exact verb against a longer one
**	(SET/SETUP), synonyms, extra words, sub-verb help, the stored keyword and
**	__cli$get_keyword(), unknown qualifiers, the silent probe of an absent item
**	under CLI$M_OPSIGNAL, "/q value" without '=', date and number validation.
**
**  MODULE:  test_cli_rules.c
**--
*/
#define __MODULE__ "CLIR$"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "test_harness.h"
#include "utility_routines.h"
#include "cli_routines.h"

static int s_ran;
static int s_act_set   (CLI_CTX *a_c, void *a_a) { (void) a_c; (void) a_a; s_ran = 1; return STS$K_SUCCESS; }
static int s_act_setup (CLI_CTX *a_c, void *a_a) { (void) a_c; (void) a_a; s_ran = 2; return STS$K_SUCCESS; }
static int s_act_list  (CLI_CTX *a_c, void *a_a) { (void) a_c; (void) a_a; s_ran = 3; return STS$K_SUCCESS; }
static int s_act_stat  (CLI_CTX *a_c, void *a_a) { (void) a_c; (void) a_a; s_ran = 4; return STS$K_SUCCESS; }

static CLI_KEYWORD s_orders [] = {
	{ .name={$ASCINI("ascending")},  .val = 10 },
	{ .name={$ASCINI("descending")}, .val = 20 },
	{ .name={$ASCINI("inet")},       .val = 30 },
	{ .name={$ASCINI("inet6")},      .val = 40 },
	{ .name={0} }
};
static CLI_PQDESC s_p1 [] = {
	{ .name={$ASCINI("file")}, .type=CLI$K_FILE, .pn=CLI$K_P1 },
	{ .name={0} }
};
static CLI_PQDESC s_pdev [] = {
	{ .name={$ASCINI("dev")}, .type=CLI$K_DEVICE, .pn=CLI$K_P1 },
	{ .name={0} }
};
static CLI_PQDESC s_pnum [] = {
	{ .name={$ASCINI("n")}, .type=CLI$K_NUM, .pn=CLI$K_P1 },
	{ .name={0} }
};
static CLI_PQDESC s_q [] = {
	{ .name={$ASCINI("order")},  .type=CLI$K_KWD,  .pn=CLI$K_QUAL, .kwd=s_orders },
	{ .name={$ASCINI("blocks")}, .type=CLI$K_NUM,  .pn=CLI$K_QUAL },
	{ .name={$ASCINI("when")},   .type=CLI$K_DATE, .pn=CLI$K_QUAL },
	{ .name={$ASCINI("tags")},   .type=CLI$K_QSTRING, .pn=CLI$K_QUAL, .flag=CLI$M_LIST },
	{ .name={0} }
};
static CLI_VERB s_stat_sub [] = {
	{ .name={$ASCINI("statistics")}, .act_rtn=s_act_stat, .quals=s_q },
	{ .name={0} }
};
static CLI_VERB s_cmds [] = {
	{ .name={$ASCINI("set")},     .act_rtn=s_act_set },
	{ .name={$ASCINI("setup")},   .act_rtn=s_act_setup, .params=s_p1, .quals=s_q },
	{ .name={$ASCINI("list")},    .act_rtn=s_act_list,  .params=s_p1 },
	{ .name={$ASCINI("listing")}, .act_rtn=s_act_list,  .params=s_p1 },
	{ .name={$ASCINI("show")},    .next=s_stat_sub },
	{ .name={$ASCINI("dev")},     .act_rtn=s_act_set, .params=s_pdev, .quals=s_q },
	{ .name={$ASCINI("num")},     .act_rtn=s_act_set, .params=s_pnum, .quals=s_q },
	{ .name={$ASCINI("sort")},    .act_rtn=s_act_set, .params=s_p1, .quals=s_q },
	{ .name={0} }
};

static int asc_eq (ASC *a_asc, const char *a_str)
{
	return	(a_asc->len == strlen(a_str)) && (0 == memcmp(a_asc->sts, a_str, a_asc->len));
}

static int s_logfd;
static char s_name[] = "/tmp/clir-XXXXXX";

static void s_capture_begin (void)
{
	s_logfd = mkstemp(s_name);
	__util$setlogfd(s_logfd);
	fflush(stdout);
}

/* Returns the captured log text; the help screen goes to the log destination too */
static char * s_capture_end (char *a_buf, size_t a_sz)
{
ssize_t	l_n;

	lseek(s_logfd, 0, SEEK_SET);
	l_n = read(s_logfd, a_buf, a_sz - 1);
	a_buf[l_n > 0 ? l_n : 0] = '\0';
	close(s_logfd);
	unlink(s_name);
	strcpy(s_name, "/tmp/clir-XXXXXX");
	__util$setlogfd(2);
	return	a_buf;
}

static int s_parse (int a_opts, int a_argc, char **a_argv, void **a_ctx)
{
	return	__cli$parse(s_cmds, a_opts, a_argc, a_argv, a_ctx);
}

int main (void)
{
void		*l_ctx;
ASC		l_v;
CLI_KEYWORD	*l_kwd;
char		l_log[16384];
int		l_st;

	printf("== cli rules regression ==\n");

	/* --- SET versus SETUP --- */
	{
	char *l_a[] = { "set" };
	s_ran = 0;
	CHECK( 1 & s_parse(0, 1, l_a, &l_ctx), "'set' must parse although SETUP exists");
	__cli$dispatch(l_ctx);
	CHECK( s_ran == 1, "'set' is SET, not SETUP (ran=%d)", s_ran);
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "SET" };
	s_ran = 0;
	CHECK( 1 & s_parse(0, 1, l_a, &l_ctx), "'SET' parses");
	__cli$dispatch(l_ctx);
	CHECK( s_ran == 1, "'SET' in upper case is SET too");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "setu" };
	s_ran = 0;
	CHECK( 1 & s_parse(0, 1, l_a, &l_ctx), "'setu' parses");
	__cli$dispatch(l_ctx);
	CHECK( s_ran == 2, "'setu' is SETUP (ran=%d)", s_ran);
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "se" };
	CHECK( !(1 & s_parse(0, 1, l_a, &l_ctx)), "'se' matches SET and SETUP without being either: ambiguous");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "s" };
	CHECK( !(1 & s_parse(0, 1, l_a, &l_ctx)), "'s' is ambiguous");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "nosuch" };
	CHECK( !(1 & s_parse(0, 1, l_a, &l_ctx)), "an unknown verb is rejected");
	__cli$cleanup(l_ctx);
	}
	/* synonyms: LIST and LISTING are one command; "lis" is not ambiguous, "list" is LIST in full */
	{
	char *l_a[] = { "lis", "x.txt" };
	s_ran = 0;
	CHECK( 1 & s_parse(0, 2, l_a, &l_ctx), "'lis' is not ambiguous between synonyms LIST and LISTING");
	__cli$dispatch(l_ctx);
	CHECK( s_ran == 3, "synonyms run the common action");
	__cli$cleanup(l_ctx);
	}

	/* --- extra words --- */
	{
	char *l_a[] = { "set", "extra" };
	CHECK( !(1 & s_parse(0, 2, l_a, &l_ctx)), "a verb with no params and quals rejects an extra word");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "list", "a.txt", "b.txt" };
	CHECK( !(1 & s_parse(0, 3, l_a, &l_ctx)), "a surplus word after the last parameter is rejected");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "set", "/help" };
	CHECK( STS$K_WARN == s_parse(0, 2, l_a, &l_ctx), "'set /help' still asks for help, not 'too many parameters'");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "set", "extra" };
	s_capture_begin();
	s_parse(CLI$M_OPSIGNAL, 2, l_a, &l_ctx);
	s_capture_end(l_log, sizeof(l_log));
	CHECK( strstr(l_log, "Too many parameters"), "the message says 'Too many parameters'");
	__cli$cleanup(l_ctx);
	}

	/* --- sub-verb help is reachable --- */
	{
	char *l_a[] = { "show", "statistics", "/help" };
	s_capture_begin();
	l_st = s_parse(0, 3, l_a, &l_ctx);
	s_capture_end(l_log, sizeof(l_log));
	CHECK( l_st == STS$K_WARN, "'show statistics /help' gives help (WARN)");
	CHECK( strstr(l_log, "Usage: statistics") && strstr(l_log, "/order"), "the help of the sub-verb, with its qualifiers, is shown:\n%s", l_log);
	CHECK( !strstr(l_log, "Subcommands"), "not the list of the container's sub-verbs");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "show", "/help" };
	s_capture_begin();
	l_st = s_parse(0, 2, l_a, &l_ctx);
	s_capture_end(l_log, sizeof(l_log));
	CHECK( (l_st == STS$K_WARN) && strstr(l_log, "Subcommands"), "'show /help' lists the sub-verbs");
	__cli$cleanup(l_ctx);
	}

	/* --- keyword stored in full; get_keyword --- */
	{
	char *l_a[] = { "sort", "f", "/order=desc" };
	CHECK( 1 & s_parse(0, 3, l_a, &l_ctx), "/order=desc parses");
	CHECK( (1 & __cli$get_value(l_ctx, &s_q[0], &l_v)) && asc_eq(&l_v, "descending"), "the keyword is stored in full ('descending')");
	CHECK( (1 & __cli$get_keyword(l_ctx, &s_q[0], &l_v, &l_kwd)) && l_kwd && (l_kwd->val == 20) && (l_kwd == &s_orders[1]), "get_keyword returns the table entry, val 20");
	CHECK( (1 & __cli$get_keyword(l_ctx, &s_q[0], NULL, &l_kwd)) && (l_kwd->val == 20), "get_keyword with NULL value takes the item's first");
	CHECK( !(1 & __cli$get_keyword(l_ctx, &s_q[1], NULL, &l_kwd)), "get_keyword on a non-keyword item fails");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "f", "/order=" };
	l_st = s_parse(0, 3, l_a, &l_ctx);
	CHECK( (1 & l_st) || (STS$K_WARN == l_st), "'/order=' parses (%d)", l_st);
	CHECK( STS$K_WARN == __cli$get_keyword(l_ctx, &s_q[0], NULL, &l_kwd) && !l_kwd, "get_keyword on a present /order= (empty): STS$K_WARN, no entry");
	CHECK( STS$K_WARN == __cli$get_value(l_ctx, &s_q[0], &l_v), "...the same as get_value says");
	CHECK( STS$K_WARN == __cli$get_keyword(l_ctx, &s_q[0], &l_v, &l_kwd), "get_keyword with the empty value of get_value: STS$K_WARN");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "f", "/order" };
	l_st = s_parse(0, 3, l_a, &l_ctx);
	CHECK( (1 & l_st) || (STS$K_WARN == l_st), "'/order' parses (%d)", l_st);
	CHECK( STS$K_WARN == __cli$get_keyword(l_ctx, &s_q[0], NULL, &l_kwd), "get_keyword on a present /order without a value: STS$K_WARN");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "f" };
	CHECK( 1 & s_parse(0, 2, l_a, &l_ctx), "no /order at all");
	CHECK( STS$K_ERROR == __cli$get_keyword(l_ctx, &s_q[0], NULL, &l_kwd) && !l_kwd, "get_keyword on an absent /order: STS$K_ERROR");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "f", "/order=INET" };
	CHECK( (1 & s_parse(0, 3, l_a, &l_ctx)) && (1 & __cli$get_keyword(l_ctx, &s_q[0], NULL, &l_kwd)) && (l_kwd->val == 30), "INET in full wins over INET6 (val 30)");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "f", "/order=a" };
	CHECK( (1 & s_parse(0, 3, l_a, &l_ctx)) && (1 & __cli$get_value(l_ctx, &s_q[0], &l_v)) && asc_eq(&l_v, "ascending"), "'a' is stored as 'ascending'");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "f", "/order=inet" , "/order=x" };
	CHECK( !(1 & s_parse(0, 4, l_a, &l_ctx)), "an unknown keyword is rejected");
	__cli$cleanup(l_ctx);
	}

	/* --- unknown qualifiers --- */
	{
	char *l_a[] = { "sort", "--bogus" };
	CHECK( !(1 & s_parse(0, 2, l_a, &l_ctx)), "'--bogus' is not the optional P1");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "num", "/BOGUS" };
	CHECK( !(1 & s_parse(0, 2, l_a, &l_ctx)), "'/BOGUS' is not taken for a NUM parameter");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "num", "/bogus=1" };
	s_capture_begin();
	l_st = s_parse(CLI$M_OPSIGNAL, 2, l_a, &l_ctx);
	s_capture_end(l_log, sizeof(l_log));
	CHECK( !(1 & l_st) && strstr(l_log, "Unrecognized qualifier"), "'/bogus=1' gives 'Unrecognized qualifier'");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "/etc/passwd" };
	CHECK( (1 & s_parse(0, 2, l_a, &l_ctx)) && (1 & __cli$get_value(l_ctx, &s_p1[0], &l_v)) && asc_eq(&l_v, "/etc/passwd"), "a path with a second '/' stays a parameter");
	__cli$cleanup(l_ctx);
	}

	/* --- "/name" which matches no qualifier: a path for a pending FILE/DEVICE parameter, else an error --- */
	{
	char *l_a[] = { "sort", "/tmp" };
	CHECK( (1 & s_parse(0, 2, l_a, &l_ctx)) && (1 & __cli$get_value(l_ctx, &s_p1[0], &l_v)) && asc_eq(&l_v, "/tmp"), "'/tmp' is the value of a pending FILE parameter");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "--tmp" };
	CHECK( !(1 & s_parse(0, 2, l_a, &l_ctx)), "'--tmp' is an error even for a FILE parameter");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "dev", "/null" };
	CHECK( (1 & s_parse(0, 2, l_a, &l_ctx)) && (1 & __cli$get_value(l_ctx, &s_pdev[0], &l_v)) && asc_eq(&l_v, "/null"), "a /name is the value of a pending DEVICE parameter");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "num", "/bogus" };
	CHECK( !(1 & s_parse(0, 2, l_a, &l_ctx)), "'/bogus' for a NUM parameter is an error");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "num", "5", "/tmp" };
	CHECK( !(1 & s_parse(0, 3, l_a, &l_ctx)), "'/tmp' with no parameter pending is an error");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "f", "/tmp" };
	CHECK( !(1 & s_parse(0, 3, l_a, &l_ctx)), "'/tmp' after the only FILE parameter is filled is an error");
	__cli$cleanup(l_ctx);
	}
	{
	char *l_a[] = { "sort", "/ord=desc" };
	CHECK( (1 & s_parse(0, 2, l_a, &l_ctx)) && (1 & __cli$get_keyword(l_ctx, &s_q[0], NULL, &l_kwd)) && (l_kwd->val == 20), "'/ord' stays the /order qualifier, not a path");
	__cli$cleanup(l_ctx);
	}

	/* --- the silent probe --- */
	{
	char *l_a[] = { "sort", "f", "/tags=(a,b)" };
	CHECK( 1 & s_parse(CLI$M_OPSIGNAL, 3, l_a, &l_ctx), "parse with OPSIGNAL");
	s_capture_begin();
	l_st = __cli$get_value(l_ctx, &s_q[1], &l_v);			/* /blocks absent */
	CHECK( !(1 & l_st), "absent /blocks: a non-success status (%d)", l_st);
	CHECK( (1 & __cli$get_value(l_ctx, &s_q[3], &l_v)) && (1 & __cli$get_value(l_ctx, &s_q[3], &l_v)), "two list elements");
	CHECK( !(1 & __cli$get_value(l_ctx, &s_q[3], &l_v)), "the drained list reports absence");
	s_capture_end(l_log, sizeof(l_log));
	CHECK( !l_log[0], "OPSIGNAL: probing an absent item / drained list logs nothing, got '%s'", l_log);
	__cli$cleanup(l_ctx);
	}

	/* --- '/blocks 5' needs '=' --- */
	{
	char *l_a[] = { "sort", "f", "/blocks", "5" };
	s_capture_begin();
	l_st = s_parse(CLI$M_OPSIGNAL, 4, l_a, &l_ctx);
	s_capture_end(l_log, sizeof(l_log));
	CHECK( !(1 & l_st), "'/blocks 5' stays an error");
	CHECK( strstr(l_log, "must follow '='"), "the message says the value must follow '=', got '%s'", l_log);
	__cli$cleanup(l_ctx);
	}

	/* --- numbers --- */
	{
	static const struct { const char *v; int ok; } l_n[] = {
		{ "5", 1 }, { "0x1f", 1 }, { "017", 1 }, { "18446744073709551615", 1 },
		{ "18446744073709551616", 0 }, { "-1", 0 }, { "-0", 0 }, { " 5", 0 }, { "5x", 0 }, { "09", 0 }, { "", 1 }, { "0x", 0 }, { "1 2", 0 } };

	for (unsigned i = 0; i < sizeof(l_n) / sizeof(l_n[0]); i++)
		{
		char	l_arg[64], *l_a[] = { "sort", "f", l_arg };

		snprintf(l_arg, sizeof(l_arg), "/blocks=%s", l_n[i].v);
		l_st = s_parse(0, 3, l_a, &l_ctx);
		CHECK( !!(1 & l_st) == l_n[i].ok, "/blocks=%s must %s", l_n[i].v, l_n[i].ok ? "pass" : "fail");
		__cli$cleanup(l_ctx);
		}
	}

	/* --- dates --- */
	{
	static const struct { const char *v; int ok; } l_d[] = {
		{ "31-12-2026", 1 }, { "01-01-2026-00:00:00", 1 }, { "31-12-2026-23:59:59", 1 }, { "31-12-2026 23:59", 1 }, { "31-12-2026T10:20:30", 1 },
		{ "29-02-2024", 1 }, { "29-02-2023", 0 }, { "29-02-1900", 0 }, { "29-02-2000", 1 },
		{ "31-04-2026", 0 }, { "32-01-2026", 0 }, { "00-01-2026", 0 }, { "10-00-2026", 0 }, { "10-13-2026", 0 }, { "10-99-2026", 0 },
		{ "10-10-2026-24:00:00", 0 }, { "10-10-2026-12:60:00", 0 }, { "10-10-2026-12:30:60", 0 }, { "10-10-2026-12", 0 },
		{ "10-10-2026x", 0 }, { "10-10-2026-12:30:00x", 0 }, { "10-10", 0 }, { "abc", 0 }, { "-5-10-2026", 0 }, { "10-10-0000", 0 } };

	for (unsigned i = 0; i < sizeof(l_d) / sizeof(l_d[0]); i++)
		{
		char	l_arg[64], *l_a[] = { "sort", "f", l_arg };

		snprintf(l_arg, sizeof(l_arg), "/when=%s", l_d[i].v);
		l_st = s_parse(0, 3, l_a, &l_ctx);
		CHECK( !!(1 & l_st) == l_d[i].ok, "/when=%s must %s", l_d[i].v, l_d[i].ok ? "pass" : "fail");
		__cli$cleanup(l_ctx);
		}
	}

	/* --- show_verbs does not change what it was given; and show_ctx is callable --- */
	{
	char *l_a[] = { "sort", "f", "/blocks=1" };
	s_capture_begin();
	__cli$show_verbs(s_cmds, 0);
	__cli$parse(0, 0, 0, NULL, &l_ctx);
	if ( 1 & s_parse(0, 3, l_a, &l_ctx) )
		__cli$show_ctx(l_ctx);
	s_capture_end(l_log, sizeof(l_log));
	CHECK( strstr(l_log, "Show verb 'show'") && strstr(l_log, "statistics"), "show_verbs lists the sub-verb table");
	CHECK( strstr(l_log, "P1[0:1]='f'") && strstr(l_log, "/blocks[0:1]='1'"), "show_ctx tells parameters from qualifiers:\n%s", l_log);
	__cli$cleanup(l_ctx);
	}

	TEST_DONE();
}
