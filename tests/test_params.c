/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:	__util$getparams(), __util$readconfig() and __util$showparams() with an OPTS table.
**	Pinned: OPTS$K_INT stores exactly the size of the option and nothing around it; a bad or
**	oversize number is an error; sizes 1, 2, 8 are unsigned (no minus sign) and 4 and 0 signed (int32); matching quotes around a value are dropped; OPTS$K_PORT is an unsigned short in host order; a full name
**	wins and a unique abbreviation selects; argv is never written; bad value, unknown option
**	or stray word give an error status while the rest is still processed; a 255-octet text
**	keeps its NUL; OPTS$K_CONF names a file to read and is rejected inside one.
**
**  AUTHOR:  StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  31-OCT-2026
**
**  MODULE:  test_params.c
**--
*/
#define _GNU_SOURCE
#define __MODULE__ "PART$"

#ifdef	_WIN32
#include <stdio.h>

int main (void)
{
	printf("== options regression: skipped, it needs POSIX (mkstemp, pread, arpa/inet.h) ==\n");
	return	0;
}
#else
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "test_harness.h"
#include "utility_routines.h"

/* One OPTS entry, the name ASC braced - $ASCINI() leans on brace elision, which -Wmissing-braces dislikes */
#define	OPT_END	{ {0, {0}}, NULL, 0, 0 }
#define	OPT(n, p, sz, ty)	{ {(unsigned char) (sizeof(n) - 1), {n}}, (p), (sz), (ty) }

static int	s_logfd = -1;
static char	s_logname [] = "/tmp/test_params_XXXXXX";

/* Forget what has been logged so far */
static void s_logreset (void)
{
	if ( ftruncate(s_logfd, 0) || (0 > lseek(s_logfd, 0, SEEK_SET)) )
		printf("  (log file reset failed)\n");
}

/* Everything logged since the reset, as a string */
static const char *s_logtext (void)
{
static char	l_buf [8192];
ssize_t	l_n = pread(s_logfd, l_buf, sizeof(l_buf) - 1, 0);

	l_buf[(l_n > 0) ? l_n : 0] = '\0';
	return	l_buf;
}

static void s_writefile (const char *a_path, const char *a_text)
{
FILE	*l_f = fopen(a_path, "w");

	fputs(a_text, l_f);
	fclose(l_f);
}

/* ---------------------------------------------------------------------------------------------
** The INT sizes: a window of 16 octets filled with 0xAA, the option at offset 4
*/
static void s_sizes (void)
{
unsigned char	l_w [16];
OPTS	l_tbl [] = {
		OPT("val", l_w + 4, 0, OPTS$K_INT),
		OPT_END
	};
static const struct	{ size_t sz; const char *arg; int ok; unsigned long long v; }	l_v [] = {
		{ 1, "-val=0x7F",		1, 0x7F },
		{ 1, "-val=255",		1, 255 },
		{ 1, "-val=256",		0, 0 },
		{ 1, "-val=-1",		0, 0 },
		{ 1, "-val=-0",		0, 0 },
		{ 1, "-val=+7",		1, 7 },
		{ 1, "-val=-129",		0, 0 },
		{ 2, "-val=65535",		1, 65535 },
		{ 2, "-val=65536",		0, 0 },
		{ 2, "-val=-2",		0, 0 },
		{ 2, "-val=0x1234",		1, 0x1234 },
		{ 4, "-val=0xFFFFFFFF",	0, 0 },
		{ 4, "-val=0x7FFFFFFF",	1, 0x7FFFFFFFULL },
		{ 4, "-val=2147483648",	0, 0 },
		{ 4, "-val=-2147483648",	1, 0x80000000ULL },
		{ 4, "-val=-2147483649",	0, 0 },
		{ 4, "-val=4294967296",	0, 0 },
		{ 4, "-val=-5",		1, 0xFFFFFFFBULL },
		{ 4, "-val=010",		1, 8 },
		{ 8, "-val=0xFFFFFFFFFFFFFFFF",1, 0xFFFFFFFFFFFFFFFFULL },
		{ 8, "-val=-1",		0, 0 },
		{ 8, "-val=99999999999",	1, 99999999999ULL },
		{ 8, "-val=18446744073709551616", 0, 0 },
		{ 4, "-val=abc",		0, 0 },
		{ 4, "-val=",			0, 0 },
		{ 4, "-val=12x",		0, 0 },
		{ 4, "-val",			0, 0 },
		{ 4, "-val=  42  ",		1, 42 },
	};
char	*l_argv [3];
int	l_i, l_k, l_sts, l_bad = 0, l_badedge = 0;
unsigned long long l_got;

	for ( l_i = 0; l_i < (int) (sizeof(l_v)/sizeof(l_v[0])); l_i++ )
		{
		memset(l_w, 0xAA, sizeof(l_w));
		l_tbl[0].sz = l_v[l_i].sz;
		l_argv[0] = "prog";
		l_argv[1] = (char *) l_v[l_i].arg;
		l_argv[2] = NULL;

		s_logreset();
		l_sts = __util$getparams(2, l_argv, l_tbl);

		l_got = 0;
		memcpy(&l_got, l_w + 4, l_v[l_i].sz);		/* little-endian host: the low octets */

		if ( l_v[l_i].ok )
			{
			if ( !(1 & l_sts) || (l_got != l_v[l_i].v) )
				{
				l_bad++;
				printf("  size %u '%s': status %d value %llx (expected %llx)\n", (unsigned) l_v[l_i].sz, l_v[l_i].arg, l_sts, l_got, l_v[l_i].v);
				}
			}
		else if ( (1 & l_sts) || !strstr(s_logtext(), "val") )
			{
			l_bad++;
			printf("  size %u '%s': a bad value gave status %d, log '%s'\n", (unsigned) l_v[l_i].sz, l_v[l_i].arg, l_sts, s_logtext());
			}

		/* Not an octet outside the option's own size has been touched */
		for ( l_k = 0; l_k < 16; l_k++ )
			if ( ((l_k < 4) || (l_k >= 4 + (int) l_v[l_i].sz)) && (l_w[l_k] != 0xAA) )
				l_badedge++;
		}
	CHECK( 0 == l_bad, "OPTS$K_INT of 1/2/4/8 octets: values, ranges, bad values (%d wrong)", l_bad);
	CHECK( 0 == l_badedge, "...and not an octet around the option is written (%d changed)", l_badedge);

	/* sz 0 is an int, as the tables of the programs have it */
	{
	struct	{ unsigned char pre [4]; int v; unsigned char post [4]; } l_s;
	OPTS	l_t2 [] = { OPT("n", &l_s.v, 0, OPTS$K_INT), OPT_END };
	char	*l_a2 [] = { "prog", "-n=-77", NULL };

	memset(&l_s, 0xAA, sizeof(l_s));
	l_sts = __util$getparams(2, l_a2, l_t2);
	CHECK( (1 & l_sts) && (-77 == l_s.v) && (0xAA == l_s.pre[3]) && (0xAA == l_s.post[0]), "sz 0 means int");
	}

	/* sz 0 is signed, the range is int32 */
	{
	int	l_n = 0;
	OPTS	l_t5 [] = { OPT("n", &l_n, 0, OPTS$K_INT), OPT_END };
	char	*l_a5 [] = { "prog", "-n=2147483648", NULL };

	l_sts = __util$getparams(2, l_a5, l_t5);
	CHECK( !(1 & l_sts) && (0 == l_n), "sz 0: 2147483648 is out of int32");
	l_a5[1] = "-n=-2147483648";
	l_sts = __util$getparams(2, l_a5, l_t5);
	CHECK( (1 & l_sts) && (INT32_MIN == l_n), "sz 0: -2147483648 is the lowest int");
	}

	/* PORT: an unsigned short in host order */
	{
	struct	{ unsigned short pre; unsigned short port; unsigned short post; } l_p = { 0xAAAA, 0, 0xAAAA };
	OPTS	l_t3 [] = { OPT("port", &l_p.port, 0, OPTS$K_PORT), OPT_END };
	char	*l_a3 [] = { "prog", "-port=4020", NULL }, *l_a4 [] = { "prog", "-port=70000", NULL };

	l_sts = __util$getparams(2, l_a3, l_t3);
	CHECK( (1 & l_sts) && (4020 == l_p.port) && (0xAAAA == l_p.pre) && (0xAAAA == l_p.post), "OPTS$K_PORT: 4020 is stored as the number 4020 (host order), two octets");
	l_sts = __util$getparams(2, l_a4, l_t3);
	CHECK( !(1 & l_sts) && (4020 == l_p.port), "OPTS$K_PORT: 70000 is a bad value and leaves the variable alone");
	}
}

/* ---------------------------------------------------------------------------------------------
** Names: full wins, a unique prefix selects, an ambiguous one is an error; the vector is not written
*/
static void s_names (void)
{
int	l_ns = 0, l_ns6 = 0, l_trace = 0;
ASC	l_logf = {0};
OPTS	l_tbl [] = {
		OPT("nserver", &l_ns,		0, OPTS$K_INT),
		OPT("nserver6", &l_ns6,		0, OPTS$K_INT),
		OPT("trace", &l_trace,	0, OPTS$K_OPT),
		OPT("logfile", &l_logf,	ASC$K_SZ, OPTS$K_STR),
		OPT_END
	};
int	l_sts;

	{ char *l_a[] = { "prog", "-nserver=1", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( (1 & l_sts) && (1 == l_ns) && (0 == l_ns6), "'-nserver' is the full name of nserver, though it begins nserver6 too");

	{ char *l_a[] = { "prog", "-nserver6=2", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( (1 & l_sts) && (1 == l_ns) && (2 == l_ns6), "'-nserver6' is nserver6");

	s_logreset();
	{ char *l_a[] = { "prog", "-nse=3", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( !(1 & l_sts) && (1 == l_ns) && (2 == l_ns6), "'-nse' is ambiguous: an error, nothing is changed");
	CHECK( NULL != strstr(s_logtext(), "nse"), "...and the log names the abbreviation");

	{ char *l_a[] = { "prog", "-tr", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( (1 & l_sts) && (1 == l_trace), "'-tr' is a unique abbreviation of trace");

	{ char *l_a[] = { "prog", "--LOGF=first", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( (1 & l_sts) && (5 == l_logf.len) && !strcmp(l_logf.sts, "first"), "'--LOGF' - two dashes, any case, abbreviated");

	{ char *l_a[] = { "prog", "/log=second", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( (1 & l_sts) && !strcmp(l_logf.sts, "second"), "'/log' - a slash and the shortest unique prefix");

	s_logreset();
	{ char *l_a[] = { "prog", "-nserverxyz=1", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( !(1 & l_sts) && (1 == l_ns), "'-nserverxyz' is longer than any name: unknown, an error");
	CHECK( NULL != strstr(s_logtext(), "nserverxyz"), "...and it is logged");

	/* The vector is read-only: string literals, white space around the value, a value without '=' */
	{
	char	*l_a[] = { "prog", "-logfile=  spaced value \t", "-trace", NULL };
	const char	*l_copy [3] = { l_a[0], l_a[1], l_a[2] };
	char	l_saved [64];

	strcpy(l_saved, l_a[1]);
	l_trace = 0;
	l_sts = __util$getparams(3, l_a, l_tbl);
	CHECK( (1 & l_sts) && !strcmp(l_logf.sts, "spaced value") && (12 == l_logf.len), "the value is trimmed in the option: 'spaced value'");
	CHECK( (l_a[1] == l_copy[1]) && !strcmp(l_a[1], l_saved) && (1 == l_trace), "...but the argv string is as it was (and a string literal would crash a writer)");
	}
}

/* ---------------------------------------------------------------------------------------------
** The status after a problem; the rest is still processed
*/
static void s_status (void)
{
int	l_count = 0, l_trace = 0;
struct in_addr	l_ip = { 0 };
OPTS	l_tbl [] = {
		OPT("count", &l_count,	0, OPTS$K_INT),
		OPT("trace", &l_trace,	0, OPTS$K_OPT),
		OPT("ip", &l_ip,		0, OPTS$K_IP4),
		OPT_END
	};
int	l_sts;

	s_logreset();
	{ char *l_a[] = { "prog", "-count=5", "-trace", "-ip=10.1.2.3", NULL };	l_sts = __util$getparams(4, l_a, l_tbl); }
	CHECK( (STS$K_SUCCESS == l_sts) && (5 == l_count) && (1 == l_trace), "all good: STS$K_SUCCESS");
	CHECK( ntohl(l_ip.s_addr) == 0x0A010203, "OPTS$K_IP4 is stored in network order");
	CHECK( '\0' == *s_logtext(), "...and nothing is logged");

	l_trace = 0;
	s_logreset();
	{ char *l_a[] = { "prog", "-count=abc", "-trace", NULL };	l_sts = __util$getparams(3, l_a, l_tbl); }
	CHECK( !(1 & l_sts), "a bad value gives a status with the low bit clear (got %d)", l_sts);
	CHECK( 1 == l_trace, "...but the following option is still processed");
	CHECK( NULL != strstr(s_logtext(), "abc") && NULL != strstr(s_logtext(), "count"), "...and the problem is logged with the value and the option: '%s'", s_logtext());

	l_trace = 0;
	s_logreset();
	{ char *l_a[] = { "prog", "-bogus", "-trace", NULL };	l_sts = __util$getparams(3, l_a, l_tbl); }
	CHECK( !(1 & l_sts) && (1 == l_trace), "an unknown option: an error, the rest is processed");
	CHECK( NULL != strstr(s_logtext(), "bogus"), "...and it is logged");

	s_logreset();
	{ char *l_a[] = { "prog", "stray", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( !(1 & l_sts) && (NULL != strstr(s_logtext(), "stray")), "a word without '-' or '/' is an error and is logged");

	s_logreset();
	{ char *l_a[] = { "prog", "-ip=999.1.1.1", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( !(1 & l_sts) && (NULL != strstr(s_logtext(), "999.1.1.1")), "a bad IP4 address is an error and is logged");

	{ char *l_a[] = { "prog", "", NULL };	l_sts = __util$getparams(2, l_a, l_tbl); }
	CHECK( STS$K_SUCCESS == l_sts, "an empty argument is skipped");
}

/* ---------------------------------------------------------------------------------------------
** Texts of 255 octets and more
*/
static void s_texts (void)
{
struct	{ ASC a; unsigned char guard [8]; }	l_s, l_p;
OPTS	l_tbl [] = {
		OPT("name", &l_s.a,	ASC$K_SZ, OPTS$K_STR),
		OPT("pass", &l_p.a,	ASC$K_SZ, OPTS$K_PWD),
		OPT_END
	};
char	l_arg [512], l_arg2 [512];
char	*l_argv [4];
int	l_n, l_sts, l_bad = 0;

	for ( l_n = 253; l_n <= 300; l_n++ )
		{
		memset(&l_s, 0xA5, sizeof(l_s));
		memset(&l_p, 0xA5, sizeof(l_p));
		strcpy(l_arg, "-name=");
		memset(l_arg + 6, 'n', (size_t) l_n);
		l_arg[6 + l_n] = '\0';
		strcpy(l_arg2, "-pass=");
		memset(l_arg2 + 6, 'p', (size_t) l_n);
		l_arg2[6 + l_n] = '\0';
		l_argv[0] = "prog";
		l_argv[1] = l_arg;
		l_argv[2] = l_arg2;
		l_argv[3] = NULL;

		l_sts = __util$getparams(3, l_argv, l_tbl);

		if ( !(1 & l_sts) || (l_s.a.len != ((l_n < 254) ? l_n : 254)) || (l_s.a.sts[l_s.a.len] != '\0') || (0xA5 != l_s.guard[0])
			|| (l_p.a.len != ((l_n < 254) ? l_n : 254)) || (l_p.a.sts[l_p.a.len] != '\0') || (0xA5 != l_p.guard[0]) )
			l_bad++;
		}
	CHECK( 0 == l_bad, "STR and PWD of 253...300 octets: cut at 254, NUL inside the ASC, nothing past it (%d wrong)", l_bad);
}

/* ---------------------------------------------------------------------------------------------
** The configuration file
*/
static void s_config (void)
{
char	l_path [64], l_path2 [64];
int	l_ns = 0, l_ns6 = 0, l_trace = 0, l_cnt = 0;
unsigned short	l_port = 0, l_s16 = 0;
ASC	l_logf = {0}, l_conf = {0}, l_pass = {0};
OPTS	l_tbl [] = {
		OPT("nserver", &l_ns,		0, OPTS$K_INT),
		OPT("nserver6", &l_ns6,		0, OPTS$K_INT),
		OPT("trace", &l_trace,	0, OPTS$K_OPT),
		OPT("logfile", &l_logf,	ASC$K_SZ, OPTS$K_STR),
		OPT("port", &l_port,	0, OPTS$K_PORT),
		OPT("short", &l_s16,		2, OPTS$K_INT),
		OPT("count", &l_cnt,		0, OPTS$K_INT),
		OPT("config", &l_conf,	ASC$K_SZ, OPTS$K_CONF),
		OPT("password", &l_pass,	ASC$K_SZ, OPTS$K_PWD),
		OPT_END
	};
int	l_sts;
char	*l_argv [4];
char	l_arg [96];

	snprintf(l_path, sizeof(l_path), "%s.cfg1", s_logname);
	snprintf(l_path2, sizeof(l_path2), "%s.cfg2", s_logname);

	s_writefile(l_path,
		"! a comment line\n"
		"\n"
		"   -nserver = 7   ! trailing comment\n"
		"-nserver6=8\n"
		"/trace\n"
		"-logfile=\"a!b\" ! the marker inside the quotes is not a comment\n"
		"-port=4021\n"
		"-short=0x1234\n"
		"-cou=9\n"
		"-passw=secret\n");

	s_logreset();
	l_sts = __util$readconfig(l_path, l_tbl);
	CHECK( STS$K_SUCCESS == l_sts, "readconfig of a good file: STS$K_SUCCESS (got %d, log '%s')", l_sts, s_logtext());
	CHECK( (7 == l_ns) && (8 == l_ns6) && (1 == l_trace), "nserver and nserver6 told apart (full names), trace set");
	CHECK( !strcmp(l_logf.sts, "a!b"), "the '!' inside quotes stays in the value, the quotes are gone: %s", l_logf.sts);
	CHECK( (4021 == l_port) && (0x1234 == l_s16) && (9 == l_cnt) && !strcmp(l_pass.sts, "secret"), "port, short (2 octets), an abbreviation (cou), the password");

	/* Problems: all are logged, none stops the reading; the status is not a success */
	l_ns = l_trace = 0;
	s_writefile(l_path2,
		"-nserver=11\n"
		"-bogus=1\n"
		"-count=xyz\n"
		"junk without a dash\n"
		"-trace\n");
	s_logreset();
	l_sts = __util$readconfig(l_path2, l_tbl);
	CHECK( !(1 & l_sts), "readconfig of a file with problems: the low bit is clear (got %d)", l_sts);
	CHECK( (11 == l_ns) && (1 == l_trace), "...the good lines around the bad ones are taken");
	CHECK( strstr(s_logtext(), "bogus") && strstr(s_logtext(), "xyz") && strstr(s_logtext(), "junk"), "...and the unknown option, the bad value and the stray line are logged");

	s_writefile(l_path2, "-bogus=1\n-nserver=12\n");
	s_logreset();
	l_sts = __util$readconfig(l_path2, l_tbl);
	CHECK( (STS$K_INFO == l_sts) && (12 == l_ns) && strstr(s_logtext(), "bogus"), "an unknown option alone: STS$K_INFO (low bit set - the file may serve others), still logged");

	s_writefile(l_path2, "-nse=1\n");
	s_logreset();
	l_ns = 7;
	l_sts = __util$readconfig(l_path2, l_tbl);
	CHECK( !(1 & l_sts) && (7 == l_ns) && strstr(s_logtext(), "nse"), "an ambiguous abbreviation: an error, logged, nothing changed");

	/* OPTS$K_CONF inside a file: rejected with a message */
	s_writefile(l_path2, "-config=/nonexistent/other.cfg\n-nserver=13\n");
	s_logreset();
	l_sts = __util$readconfig(l_path2, l_tbl);
	CHECK( !(1 & l_sts) && (13 == l_ns), "OPTS$K_CONF inside a configuration file is an error, the next line is still read");
	CHECK( strstr(s_logtext(), "config") && !l_conf.len, "...it says so and no nesting happens (the ASC is not set)");

	/* A long line: a long comment costs nothing, a long value is refused, neither makes a line of its own */
	{
	FILE	*l_f = fopen(l_path2, "w");
	int	l_k;

	fputs("-nserver=21 ! ", l_f);
	for ( l_k = 0; l_k < 700; l_k++ )
		fputc('c', l_f);
	fputs("\n! ", l_f);
	for ( l_k = 0; l_k < 700; l_k++ )
		fputc('d', l_f);
	fputs("\n-trace\n", l_f);
	fclose(l_f);
	}
	l_trace = 0;
	s_logreset();
	l_sts = __util$readconfig(l_path2, l_tbl);
	CHECK( (STS$K_SUCCESS == l_sts) && (21 == l_ns) && (1 == l_trace) && !*s_logtext(), "readconfig: lines with a comment of 700 octets are read quietly (status %d, log '%.60s')", l_sts, s_logtext());

	{
	FILE	*l_f = fopen(l_path2, "w");
	int	l_k;

	fputs("-logfile=", l_f);
	for ( l_k = 0; l_k < 600; l_k++ )
		fputc('v', l_f);
	fputs("\n-nserver=22\n", l_f);
	fclose(l_f);
	}
	l_logf.len = 0;
	s_logreset();
	l_sts = __util$readconfig(l_path2, l_tbl);
	CHECK( !(1 & l_sts) && (22 == l_ns) && !l_logf.len && strstr(s_logtext(), "longer"), "readconfig: a value of 600 octets is refused (an error, logged), the next line is read");

	CHECK( !(1 & __util$readconfig("/nonexistent/none.cfg", l_tbl)), "readconfig: a missing file is an error");
	CHECK( !(1 & __util$readconfig(NULL, l_tbl)) && !(1 & __util$readconfig("", l_tbl)), "readconfig: no name is an error");

	/* OPTS$K_CONF on the command line: the file is read at once and its name kept */
	l_ns = 0;
	snprintf(l_arg, sizeof(l_arg), "-config=%s", l_path);
	l_argv[0] = "prog";
	l_argv[1] = l_arg;
	l_argv[2] = "-nserver=99";			/* after the file: wins over it */
	l_argv[3] = NULL;
	l_sts = __util$getparams(3, l_argv, l_tbl);
	CHECK( (1 & l_sts) && (99 == l_ns) && (8 == l_ns6) && (4021 == l_port), "getparams -config=<file>: the file is read where it stands, later options win");
	CHECK( !strcmp(l_conf.sts, l_path), "...and the name is kept in the ASC");

	s_logreset();
	snprintf(l_arg, sizeof(l_arg), "-config=%s", "/nonexistent/none.cfg");
	l_sts = __util$getparams(2, l_argv, l_tbl);
	CHECK( !(1 & l_sts) && strstr(s_logtext(), "none.cfg"), "getparams -config=<missing file>: an error, logged");

	unlink(l_path);
	unlink(l_path2);
}

/* ---------------------------------------------------------------------------------------------
** showparams: each size by its own width, the password masked
*/
static void s_show (void)
{
unsigned char	l_u8 = 200;
unsigned short	l_u16 = 513;
int		l_i32 = -5;
unsigned long long l_u64 = 5000000000ULL;
unsigned short	l_port = 4020;
ASC	l_str, l_pwd;
OPTS	l_tbl [] = {
		OPT("b8", &l_u8,		1, OPTS$K_INT),
		OPT("w16", &l_u16,		2, OPTS$K_INT),
		OPT("l32", &l_i32,		0, OPTS$K_INT),
		OPT("q64", &l_u64,		8, OPTS$K_INT),
		OPT("port", &l_port,	0, OPTS$K_PORT),
		OPT("str", &l_str,		ASC$K_SZ, OPTS$K_STR),
		OPT("pwd", &l_pwd,		ASC$K_SZ, OPTS$K_PWD),
		OPT_END
	};
const char	*l_t;

	__util$str2asc("hello", &l_str);
	__util$str2asc("topsecret", &l_pwd);

	s_logreset();
	CHECK( STS$K_SUCCESS == __util$showparams(l_tbl), "showparams succeeds");
	l_t = s_logtext();
	CHECK( strstr(l_t, "b8 = 200 (0x"), "showparams: a 1-octet integer: %s", l_t);
	CHECK( strstr(l_t, "w16 = 513 (0x0201)"), "showparams: a 2-octet integer");
	CHECK( strstr(l_t, "l32 = -5 (0x"), "showparams: an int");
	CHECK( strstr(l_t, "q64 = 5000000000 (0x"), "showparams: an 8-octet integer");
	CHECK( strstr(l_t, "port = 4020"), "showparams: a port");
	CHECK( strstr(l_t, "str[0:5] ='hello'"), "showparams: a string");
	CHECK( strstr(l_t, "pwd[0:9] ='<password>'") && !strstr(l_t, "topsecret"), "showparams: the password is masked");
}

/* ---------------------------------------------------------------------------------------------
** Quotes around a value: matching ones are dropped (file and command line), a lone or unbalanced one stays
*/
static void s_quotes (void)
{
char	l_path [64];
ASC	l_a = {0}, l_b = {0}, l_c = {0}, l_d = {0}, l_e = {0}, l_f = {0};
int	l_n = 0;
OPTS	l_tbl [] = {
		OPT("a", &l_a,	ASC$K_SZ, OPTS$K_STR),
		OPT("b", &l_b,	ASC$K_SZ, OPTS$K_STR),
		OPT("c", &l_c,	ASC$K_SZ, OPTS$K_STR),
		OPT("d", &l_d,	ASC$K_SZ, OPTS$K_STR),
		OPT("e", &l_e,	ASC$K_SZ, OPTS$K_STR),
		OPT("f", &l_f,	ASC$K_SZ, OPTS$K_STR),
		OPT("n", &l_n,	0, OPTS$K_INT),
		OPT_END
	};
char	*l_argv [8] = { "prog", "/a=\"x y\"", "-b='p q'", "-c=\"lone", "-d=\"mix'", "-e=\"\"", "-f=\"", "-n=\"42\"" };

	snprintf(l_path, sizeof(l_path), "%s.cfg3", s_logname);
	s_writefile(l_path,
		"-a = \"two words\"  \n"
		"-b='single q'\n"
		"-c=\"unbalanced\n"
		"-d=\"mixed'\n"
		"-e=\"\"\n"
		"-f='\n"
		"-n=\"17\"\n");
	s_logreset();
	CHECK( 1 & __util$readconfig(l_path, l_tbl), "readconfig with quoted values");
	CHECK( !strcmp(l_a.sts, "two words") && !strcmp(l_b.sts, "single q"), "file: matching quotes are dropped: '%s' '%s'", l_a.sts, l_b.sts);
	CHECK( !strcmp(l_c.sts, "\"unbalanced") && !strcmp(l_d.sts, "\"mixed'") && !strcmp(l_f.sts, "'"), "file: a lone or unbalanced quote stays: '%s' '%s' '%s'", l_c.sts, l_d.sts, l_f.sts);
	CHECK( !l_e.len && (17 == l_n), "file: \"\" is an empty value, \"17\" is a number");
	unlink(l_path);

	memset(&l_a, 0, sizeof(l_a)); memset(&l_b, 0, sizeof(l_b)); memset(&l_c, 0, sizeof(l_c));
	memset(&l_d, 0, sizeof(l_d)); memset(&l_e, 0, sizeof(l_e)); memset(&l_f, 0, sizeof(l_f));
	l_n = 0;
	CHECK( 1 & __util$getparams(8, l_argv, l_tbl), "getparams with quoted values passed through by the shell");
	CHECK( !strcmp(l_a.sts, "x y") && !strcmp(l_b.sts, "p q") && !strcmp(l_c.sts, "\"lone") && !strcmp(l_d.sts, "\"mix'") && !l_e.len && !strcmp(l_f.sts, "\"") && (42 == l_n),
		"command line: the same rule: '%s' '%s' '%s' '%s' '%s' %d", l_a.sts, l_b.sts, l_c.sts, l_d.sts, l_f.sts, l_n);
}

int main (void)
{
	printf("== options regression ==\n");

	if ( 0 > (s_logfd = mkstemp(s_logname)) )
		{
		printf("  FAIL: no temporary file\n");
		return	1;
		}
	__util$setlogfd(s_logfd);

	s_sizes();
	s_names();
	s_status();
	s_texts();
	s_config();
	s_show();
	s_quotes();

	close(s_logfd);
	unlink(s_logname);

	TEST_DONE();
}

#endif	/* _WIN32 */
