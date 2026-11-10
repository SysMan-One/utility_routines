/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  __util$pattern_match(), the iterative wildcard matcher ('*' - any run, '%' and
**	'?' - one octet; wildcards live in the pattern only).  Pinned by fixed vectors; a
**	half-million random cases against a dynamic-programming reference, wildcards in the
**	subjects too; and a pathological many-stars pattern which must come back at once.
**
**  AUTHOR:  StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  MODIFICATION HISTORY:
**
**	14-AUG-2026	RRL	Created (the iterative matcher against the recursive reference).
**	31-OCT-2026	CLD	DP reference written for the test; fuzz subjects carry wildcards; star-in-subject vectors.
**
**  MODULE:  test_pattern_match.c
**--
*/
#define __MODULE__ "PMT$"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "test_harness.h"
#include "utility_routines.h"

/*
** The reference: a dynamic-programming matcher, written independently of the library one.
** m[i][j] says whether the subject from octet i on matches the pattern from octet j on.
** Wildcards are looked for in the pattern only.
*/
#define	REF_MAX	24

static int s_ref (const char *a_s, const char *a_p)
{
unsigned char	l_m [REF_MAX][REF_MAX];
int	l_n = (int) strlen(a_s), l_k = (int) strlen(a_p), l_i, l_j;

	memset(l_m, 0, sizeof(l_m));
	l_m[l_n][l_k] = 1;

	for ( l_j = l_k - 1; l_j >= 0; l_j-- )			/* An empty subject: only stars can be left */
		l_m[l_n][l_j] = (a_p[l_j] == '*') && l_m[l_n][l_j + 1];

	for ( l_i = l_n - 1; l_i >= 0; l_i-- )
		for ( l_j = l_k - 1; l_j >= 0; l_j-- )
			{
			if ( a_p[l_j] == '*' )
				l_m[l_i][l_j] = l_m[l_i + 1][l_j] || l_m[l_i][l_j + 1];
			else if ( (a_p[l_j] == '%') || (a_p[l_j] == '?') || (a_p[l_j] == a_s[l_i]) )
				l_m[l_i][l_j] = l_m[l_i + 1][l_j + 1];
			}

	return	l_m[0][0];
}

int main (void)
{
char	l_s [16], l_p [10], l_hay [256];
long	l_i, l_bad;
int	l_k, l_sl, l_pl;
double	l_dt;
struct timespec l_t0, l_t1;

	printf("== pattern_match regression ==\n");

	/* Fixed vectors. */
	CHECK(  __util$pattern_match("eth0.100:rx", "eth0.100:rx"), "an exact literal matches");
	CHECK( !__util$pattern_match("eth0.100:rx", "eth0.100:tx"), "a differing literal does not");
	CHECK(  __util$pattern_match("eth0.100:rx", "eth0*"),       "a 'prefix*' matches");
	CHECK(  __util$pattern_match("eth0.100:rx", "*rx"),         "a '*suffix' matches");
	CHECK(  __util$pattern_match("eth0.100:rx", "*100*"),       "a '*middle*' matches");
	CHECK(  __util$pattern_match("eth0.100:rx", "eth%.1%0*"),   "the percent wildcard takes exactly one octet");
	CHECK( !__util$pattern_match("eth0", "eth0%"),              "the percent wildcard must not match the empty tail");
	CHECK(  __util$pattern_match("eth0.100:rx", "eth?.1?0*"),   "the question-mark alias takes exactly one octet");
	CHECK(  __util$pattern_match("abc", "a?c"),                 "the alias matches a single middle octet");
	CHECK( !__util$pattern_match("ac", "a?c"),                  "the alias must not match the empty run");
	CHECK( !__util$pattern_match("eth0", "eth0?"),              "the alias must not match the empty tail");
	CHECK(  __util$pattern_match("", "*"),                      "a lone star matches the empty string");
	CHECK(  __util$pattern_match("", ""),                       "empty matches empty");
	CHECK( !__util$pattern_match("a", ""),                      "an empty pattern rejects a non-empty string");
	CHECK(  __util$pattern_match("abc", "a**c"),                "adjacent stars collapse");

	/* Wildcards live in the pattern: a '*', '%' or '?' of the subject is an ordinary octet. */
	CHECK(  __util$pattern_match("*ba", "*a"),                  "'*ba' is matched by '*a': the subject's star is an ordinary octet for the pattern's star to take");
	CHECK(  __util$pattern_match("*a", "*a"),                   "'*a' matches '*a': the star of the pattern takes the star of the subject");
	CHECK(  __util$pattern_match("*", "*"),                     "a star matches a star");
	CHECK(  __util$pattern_match("a*b", "a*b"),                 "'a*b' matches 'a*b'");
	CHECK(  __util$pattern_match("a*b", "a?b"),                 "the subject's star is taken by the question-mark wildcard");
	CHECK(  __util$pattern_match("a*b", "a%b"),                 "the subject's star is taken by the percent wildcard");
	CHECK( !__util$pattern_match("*ab", "*ac"),                 "a star in the subject does not hide a mismatch after it");
	CHECK( !__util$pattern_match("*", "a"),                     "a star in the subject is not matched by a letter");
	CHECK(  __util$pattern_match("a%b", "a%b"),                 "a percent in the subject is a plain octet");
	CHECK(  __util$pattern_match("a?b", "a?b"),                 "a question mark in the subject is a plain octet");
	CHECK( !__util$pattern_match("ab", "a*b?"),                 "the tail '?' needs an octet");
	CHECK(  __util$pattern_match("xay*", "*ay*"),               "star, literal, star over a subject with a star");
	CHECK( !__util$pattern_match(NULL, "*") && !__util$pattern_match("a", NULL), "a NULL argument matches nothing");

	/* Fuzz against the reference: subjects and patterns both over {a,b,*,%,?}, the subjects up to 10
	** octets, the patterns up to 8 - the wildcard characters in the subjects are what reaches the
	** branch where a subject's star faces a pattern's star.
	*/
	srand(20260814);
	for ( l_i = 0, l_bad = 0; l_i < 500000; l_i++ )
		{
		l_sl = rand() % 11;
		l_pl = rand() % 9;
		for ( l_k = 0; l_k < l_sl; l_k++) l_s[l_k] = "aabb*%?"[rand() % 7];
		l_s[l_sl] = '\0';
		for ( l_k = 0; l_k < l_pl; l_k++) l_p[l_k] = "ab*%?"[rand() % 5];
		l_p[l_pl] = '\0';

		if ( !!__util$pattern_match(l_s, l_p) != !!s_ref(l_s, l_p) )
			l_bad++;
		}
	CHECK( 0 == l_bad, "500000 random cases agree with the dynamic-programming reference (%ld diverged)", l_bad);

	/* The pathological pattern: several stars over a long run of one octet; the matcher is bounded by |s|*|p|. */
	memset(l_hay, 'a', 200);
	l_hay[200] = '\0';
	clock_gettime(CLOCK_MONOTONIC, &l_t0);
	CHECK( !__util$pattern_match(l_hay, "a*a*a*a*a*b"), "the pathological pattern still mismatches");
	clock_gettime(CLOCK_MONOTONIC, &l_t1);
	l_dt = (l_t1.tv_sec - l_t0.tv_sec) + (l_t1.tv_nsec - l_t0.tv_nsec) * 1e-9;
	CHECK( l_dt < 0.05, "...and comes back at once, not in seconds (%.6f s)", l_dt);

	TEST_DONE();
}
