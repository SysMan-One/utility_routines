/*
**++
**
**  FACILITY:  starlet - regression tests
**
**  ABSTRACT:	Edge cases of the FAO formatter and of the binary time: a control string cut
**	short in a directive is never read past its NUL (exact-size malloc blocks, for
**	AddressSanitizer); a year above 9999; a !n<...!> field over FAO$K_FLDBUF; a short
**	parameter list; the FAO$K_* condition codes; the zero-time and delta-time rules.
**	starlet.h is deliberately the FIRST include: it must work without any help.
**
**  MODULE:  test_fao_edge.c
**
**  AUTHORS: CLD
**
**  CREATION DATE:  31-OCT-2026
**
**--
*/

#define __MODULE__ "TST$"

#include "starlet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "test_harness.h"

static char	g_buf [4096];
static unsigned short g_len;

/*  A control string in its own malloc block: one byte past the NUL is a heap overflow for ASan */
static char *t_dup (const char *s)
{
size_t	n = strlen (s) + 1;
char	*p = malloc (n);

	memcpy (p, s, n);
	return	p;
}

static int t_faol (const char *ctl, const fao_prm_t *prm, int cnt)
{
fao_desc_t	d = $FAO_DESC (g_buf);
char	*c = t_dup (ctl);
int	sts;

	memset (g_buf, 0, sizeof(g_buf));
	sts = __util$faol (c, &g_len, &d, prm, cnt);
	free (c);
	return	sts;
}

static int t_fao (const char *ctl, const char *dummy)
{
fao_desc_t	d = $FAO_DESC (g_buf);
char	*c = t_dup (ctl);
int	sts;

	memset (g_buf, 0, sizeof(g_buf));
	sts = __util$fao (c, &g_len, &d, (fao_prm_t) (uintptr_t) dummy, (fao_prm_t) (uintptr_t) dummy,
		(fao_prm_t) (uintptr_t) dummy);
	free (c);
	return	sts;
}

static int t_is (const char *want)
{
	return	(g_len == strlen (want)) && (memcmp (g_buf, want, g_len) == 0);
}

static void t_codes (void)
{
static const struct { const char *name; int v; int sev; } c [] = {
	{ "BUFOVF", FAO$K_BUFOVF, STS$K_ERROR }, { "BADDIR", FAO$K_BADDIR, STS$K_ERROR },
	{ "IVTIME", FAO$K_IVTIME, STS$K_FATAL }, { "NEGTIM", FAO$K_NEGTIM, STS$K_WARN },
	{ "NOPRM", FAO$K_NOPRM, STS$K_ERROR },   { "FLDOVF", FAO$K_FLDOVF, STS$K_ERROR } };
unsigned	i, j;

	printf ("[1] FAO$K_* condition values\n");

	for ( i = 0; i < sizeof(c)/sizeof(c[0]); i++ )
		{
		CHECK( !(1 & c[i].v), "%s must be even", c[i].name);
		CHECK( $SEV (c[i].v) == c[i].sev, "%s severity %d, want %d", c[i].name, (int) $SEV (c[i].v), c[i].sev);
		CHECK( $SEV (c[i].v) <= STS$K_FATAL, "%s severity is undefined", c[i].name);
		CHECK( c[i].v != STS$K_SUCCESS, "%s is the success code", c[i].name);

		for ( j = i + 1; j < sizeof(c)/sizeof(c[0]); j++ )
			CHECK( c[i].v != c[j].v, "%s == %s", c[i].name, c[j].name);
		}

	CHECK( FAO$K_BADDIR == 0x12, "FAO$K_BADDIR 0x12");
	CHECK( FAO$K_BUFOVF == 0x0a, "FAO$K_BUFOVF 0x0a (unchanged)");
	CHECK( FAO$K_IVTIME == 0x0c, "FAO$K_IVTIME 0x0c (unchanged)");
	CHECK( FAO$K_NEGTIM == 0x08, "FAO$K_NEGTIM 0x08 (unchanged)");
}

static void t_truncated (void)
{
static const char *l_cutz [] = { "!A", "!%", "!U", "!S", "!Z", "!X", "!O", "!5*", "!", "!5", "!2(", "!2<", "!2(!A", "!2<!%" };
static const fao_prm_t l_p [4] = { 1, 2, 3, 4 };
unsigned	i;
int	sts;

	printf ("[2] directives cut by the end of the control string\n");

	for ( i = 0; i < sizeof(l_cutz)/sizeof(l_cutz[0]); i++ )
		{
		sts = t_faol (l_cutz[i], l_p, 4);
		CHECK( !(1 & sts), "faol '%s': status %d must be a failure", l_cutz[i], sts);
		CHECK( sts == FAO$K_BADDIR, "faol '%s': status %d, want FAO$K_BADDIR", l_cutz[i], sts);

		sts = t_fao (l_cutz[i], "x");
		CHECK( sts == FAO$K_BADDIR, "fao '%s': status %d, want FAO$K_BADDIR", l_cutz[i], sts);
		}

	/* The text before the cut survives, and the first error is the one reported */
	sts = t_faol ("abc!", l_p, 4);
	CHECK( (sts == FAO$K_BADDIR) && t_is ("abc"), "'abc!': status %d out '%.*s'", sts, g_len, g_buf);

	sts = t_faol ("x=!UL y=!A", l_p, 4);
	CHECK( (sts == FAO$K_BADDIR) && t_is ("x=1 y="), "'x=!UL y=!A': status %d out '%.*s'", sts, g_len, g_buf);

	sts = t_faol ("!2(ab!)!", l_p, 4);
	CHECK( (sts == FAO$K_BADDIR) && t_is ("abab"), "'!2(ab!)!': status %d out '%.*s'", sts, g_len, g_buf);

	/* A well-formed string right at the end of the buffer still works */
	sts = t_faol ("!UL!3*-", l_p, 4);
	CHECK( (sts == STS$K_SUCCESS) && t_is ("1---"), "'!UL!3*-': status %d out '%.*s'", sts, g_len, g_buf);

	sts = t_faol ("!!", l_p, 4);
	CHECK( (sts == STS$K_SUCCESS) && t_is ("!"), "'!!': status %d out '%.*s'", sts, g_len, g_buf);

	/* A width that would overflow int is clamped, not undefined */
	sts = t_faol ("!99999999999999UL", l_p, 4);
	CHECK( (sts == STS$K_SUCCESS) || (sts == FAO$K_BUFOVF), "huge width: status %d", sts);

	/* A zero-length output buffer */
	{
	fao_desc_t	d = { 0, 0, 0, g_buf };
	char	*c = t_dup ("!");

	sts = __util$faol (c, &g_len, &d, l_p, 4);
	free (c);
	CHECK( sts == FAO$K_BADDIR, "empty buffer: status %d", sts);
	}
}

static void t_year (void)
{
static const struct { int64_t t; const char *want; } v [] = {
	{ 253402300799LL, "31-DEC-9999 23:59:59.00" },
	{ 253402300800LL, " 1-JAN-10000 00:00:00.00" },
	{ 4102444800LL,   " 1-JAN-2100 00:00:00.00" },
	{ 1LL,            " 1-JAN-1970 00:00:01.00" } };
fao_prm_t	p, pp [2];
fao_time_t	t;
unsigned	i;
int	sts;

	printf ("[3] !%%D and __util$asctim() with a year above 9999\n");

	if ( sizeof(fao_time_t) < 8 )
		{
		printf ("  (32-bit fao_time_t - skipped)\n");
		return;
		}

	for ( i = 0; i < sizeof(v)/sizeof(v[0]); i++ )
		{
		p = (fao_prm_t) v[i].t;
		sts = t_faol ("!%D", &p, 1);
		CHECK( (sts == STS$K_SUCCESS) && t_is (v[i].want), "!%%D %lld: status %d out '%.*s'", (long long) v[i].t, sts, g_len, g_buf);

		{
		fao_desc_t	d = $FAO_DESC (g_buf);

		t = (fao_time_t) v[i].t;
		sts = __util$asctim (&g_len, &d, &t, 0);
		CHECK( (sts == STS$K_SUCCESS) && t_is (v[i].want), "asctim %lld: status %d out '%.*s'", (long long) v[i].t, sts, g_len, g_buf);
		}
		}

	/* Far beyond: no read outside the digit table, no crash; the year is printed in full */
	p = (fao_prm_t) 4000000000000LL;			/* year 128000-ish */
	sts = t_faol ("!%D", &p, 1);
	CHECK( (sts == STS$K_SUCCESS) && t_is ("15-DEC-128724 07:06:40.00"), "year 12xxxx: status %d out '%.*s'", sts, g_len, g_buf);

	pp [0] = pp [1] = (fao_prm_t) INT64_MAX;
	sts = t_faol ("!%D|!%T", pp, 2);
	CHECK( (sts == STS$K_SUCCESS) || (sts == FAO$K_BUFOVF), "INT64_MAX: status %d", sts);

	pp [0] = pp [1] = (fao_prm_t) INT64_MIN;
	sts = t_faol ("!%D|!%T", pp, 2);
	CHECK( (sts == STS$K_SUCCESS) || (sts == FAO$K_BUFOVF), "INT64_MIN: status %d", sts);
}

static void t_field (void)
{
fao_prm_t	p [2];
static char	l_big [2000];
int	sts;

	printf ("[4] !n<...!> field over FAO$K_FLDBUF\n");

	memset (l_big, 'x', sizeof(l_big));

	p [0] = 512; p [1] = (fao_prm_t) (uintptr_t) l_big;
	sts = t_faol ("!600<!AD!>|", p, 2);
	CHECK( sts == STS$K_SUCCESS, "512 bytes fit exactly: status %d", sts);
	CHECK( g_len == 601, "512 bytes: out %d", (int) g_len);

	p [0] = 513;
	sts = t_faol ("!600<!AD!>|", p, 2);
	CHECK( sts == FAO$K_FLDOVF, "513 bytes: status %d, want FAO$K_FLDOVF", sts);
	CHECK( !(1 & sts), "FAO$K_FLDOVF must read as failure");
	CHECK( g_len == 601, "513 bytes: out %d (field cut to 512, padded to 600)", (int) g_len);
	CHECK( (g_buf [511] == 'x') && (g_buf [512] == ' ') && (g_buf [600] == '|'), "513 bytes: layout");

	p [0] = 2000;
	sts = t_faol ("!1000<!AD!>|", p, 2);
	CHECK( sts == FAO$K_FLDOVF, "2000 bytes, field 1000: status %d", sts);
	CHECK( g_len == 1001, "2000 bytes, field 1000: out %d", (int) g_len);

	/* A buffer overflow of the OUTPUT is still FAO$K_BUFOVF, not FLDOVF */
	{
	char	l_small [8];
	fao_desc_t	d = $FAO_DESC (l_small);

	p [0] = 5;
	sts = __util$faol ("!20<!AD!>", &g_len, &d, p, 2);
	CHECK( sts == FAO$K_BUFOVF, "output buffer too small: status %d", sts);
	}
}

static void t_noprm (void)
{
static const fao_prm_t l_p [3] = { 5, 6, 7 };
int	sts;

	printf ("[5] too few parameters\n");

	sts = t_faol ("!UL !UL", l_p, 1);
	CHECK( sts == FAO$K_NOPRM, "1 of 2: status %d, want FAO$K_NOPRM", sts);
	CHECK( t_is ("5 0"), "1 of 2: out '%.*s' (missing value reads as 0)", g_len, g_buf);

	sts = t_faol ("!UL !UL", l_p, 2);
	CHECK( (sts == STS$K_SUCCESS) && t_is ("5 6"), "2 of 2: status %d out '%.*s'", sts, g_len, g_buf);

	sts = t_faol ("!UL", l_p, 3);
	CHECK( sts == STS$K_SUCCESS, "spare parameters are fine: status %d", sts);

	sts = t_faol ("!UL", NULL, 0);
	CHECK( sts == FAO$K_NOPRM, "no list at all: status %d", sts);

	sts = t_faol ("!AS|!AZ|!AC|!%M|!%D", l_p, 0);
	CHECK( sts == FAO$K_NOPRM, "pointer directives without parameters: status %d", sts);
	CHECK( strstr (g_buf, "(null)") != NULL, "NULL-safe: '%.*s'", g_len, g_buf);

	sts = t_faol ("!2(!UL!)", l_p, 1);
	CHECK( sts == FAO$K_NOPRM, "repeat group: status %d", sts);

	sts = t_faol ("!#UL", l_p, 0);
	CHECK( sts == FAO$K_NOPRM, "'#' width: status %d", sts);

	sts = t_faol ("!3+!UL", l_p, 3);
	CHECK( sts == FAO$K_NOPRM, "skip past the end: status %d", sts);

	sts = t_faol ("!UL!-!UL", l_p, 1);
	CHECK( (sts == STS$K_SUCCESS) && t_is ("55"), "reuse: status %d out '%.*s'", sts, g_len, g_buf);

	/* The error is first-wins: a malformed directive is not replaced by NOPRM */
	sts = t_faol ("!UL!UL!", l_p, 1);
	CHECK( sts == FAO$K_BADDIR, "BADDIR before NOPRM: status %d", sts);
}

static void t_time (void)
{
fao_prm_t	p, p2 [2];
fao_time_t	t, tn;
fao_desc_t	d = $FAO_DESC (g_buf);
char	l_a [64], l_b [64];
unsigned short	la, lb;
int	sts;
unsigned	i;
static const int64_t l_vals [] = { 1, 59, 86399, 86400, 1700000000LL, -1, -59, -3600, -86400, -90061, -1000000 };

	printf ("[6] zero is \"now\", negative is a delta, !%%D agrees with __util$asctim()\n");

	if ( sizeof(fao_time_t) < 8 )
		{
		printf ("  (32-bit fao_time_t - skipped)\n");
		return;
		}

	/* !%D and asctim give byte-identical text for every value, both flags */
	for ( i = 0; i < sizeof(l_vals)/sizeof(l_vals[0]); i++ )
		{
		fao_desc_t	da = { sizeof(l_a), 0, 0, l_a }, db = { sizeof(l_b), 0, 0, l_b };

		t = (fao_time_t) l_vals [i];
		p = (fao_prm_t) (int64_t) l_vals [i];

		sts = __util$asctim (&la, &da, &t, 0);
		CHECK( sts == STS$K_SUCCESS, "asctim %lld", (long long) l_vals [i]);
		sts = __util$faol ("!%D", &lb, &db, &p, 1);
		CHECK( (sts == STS$K_SUCCESS) && (la == lb) && (memcmp (l_a, l_b, la) == 0),
			"!%%D vs asctim %lld: '%.*s' / '%.*s'", (long long) l_vals [i], la, l_a, lb, l_b);

		sts = __util$asctim (&la, &da, &t, 1);
		sts = __util$faol ("!%T", &lb, &db, &p, 1);
		CHECK( (sts == STS$K_SUCCESS) && (la == lb) && (memcmp (l_a, l_b, la) == 0),
			"!%%T vs asctim %lld: '%.*s' / '%.*s'", (long long) l_vals [i], la, l_a, lb, l_b);
		}

	/* Delta text */
	p = (fao_prm_t) (int64_t) -90061;
	t_faol ("!%D", &p, 1);
	CHECK( t_is ("1 01:01:01.00"), "delta !%%D: '%.*s'", g_len, g_buf);
	t_faol ("!%T", &p, 1);
	CHECK( t_is ("01:01:01.00"), "delta !%%T: '%.*s'", g_len, g_buf);
	p = (fao_prm_t) (int64_t) -1;
	t_faol ("!%D", &p, 1);
	CHECK( t_is ("0 00:00:01.00"), "delta -1 !%%D: '%.*s'", g_len, g_buf);

	/* A negative value is never a pre-1970 civil date */
	t_faol ("!%D", &p, 1);
	CHECK( strstr (g_buf, "1969") == NULL, "-1 is not 31-DEC-1969");

	/* Zero is "now": not the epoch, and equal in !%D, !%T and asctim */
	t = 0;
	sts = __util$asctim (&la, &d, &t, 0);
	CHECK( (sts == STS$K_SUCCESS) && (strncmp (g_buf, " 1-JAN-1970", 11) != 0), "asctim(0) is now: '%.*s'", la, g_buf);
	p = 0;
	t_faol ("!%D", &p, 1);
	CHECK( strncmp (g_buf, " 1-JAN-1970", 11) != 0, "!%%D of 0 is now: '%.*s'", g_len, g_buf);
	{
	fao_desc_t	da = { sizeof(l_a), 0, 0, l_a };
	fao_time_t	tz = 0;

	__util$asctim (&la, &da, NULL, 0);
	__util$asctim (&lb, &d, &tz, 0);
	CHECK( (la == lb) && (strncmp (l_a, g_buf, 8) == 0), "asctim(NULL) == asctim(&0) up to the second: '%.*s' / '%.*s'", la, l_a, lb, g_buf);
	}

	/* An absolute time before the epoch cannot be had: bintim refuses, it does not hand back a delta */
	tn = 12345;
	sts = __util$bintim ("31-DEC-1969 23:59:59", &tn);
	CHECK( (sts == FAO$K_IVTIME) && (tn == 12345), "bintim 31-DEC-1969: status %d, value %lld", sts, (long long) tn);
	sts = __util$bintim ("01-JAN-1850", &tn);
	CHECK( sts == FAO$K_IVTIME, "bintim 01-JAN-1850: status %d", sts);
	sts = __util$bintim ("01-JAN-1970 00:00:01", &tn);
	CHECK( (sts == STS$K_SUCCESS) && (tn == 1), "bintim 01-JAN-1970 00:00:01: status %d value %lld", sts, (long long) tn);
	sts = __util$bintim ("1 00:00:10", &tn);
	CHECK( (sts == STS$K_SUCCESS) && (tn == -86410), "bintim delta is negative: %lld", (long long) tn);

	/* Round trip through the text */
	sts = __util$bintim ("15-OCT-2018 15:17:13", &tn);
	t_faol ("!%D", (p2 [0] = (fao_prm_t) tn, p2), 1);
	CHECK( (sts == STS$K_SUCCESS) && t_is ("15-OCT-2018 15:17:13.00"), "round trip: '%.*s'", g_len, g_buf);
}

int main (void)
{
	t_codes ();
	t_truncated ();
	t_year ();
	t_field ();
	t_noprm ();
	t_time ();

	TEST_DONE ();
}
