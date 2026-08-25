/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  __util$pattern_match(), the iterative wildcard matcher ('*' - any
**	run, '%' - exactly one octet). The behaviour is pinned three ways: fixed
**	vectors; a half-million random cases compared against the former recursive
**	algorithm carried here verbatim as the reference; and the pathological
**	pattern that took the recursive matcher seconds must now come back at once.
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

/* The former recursive matcher, verbatim - the reference for the fuzz. */
static int s_ref (char *a_s, char *a_p)
{
int	l_done = 0, l_meos = 0;

	while ( (*a_p != '\0') && (!l_done) && (((*a_s == '\0') && (*a_p == '*')) || (*a_s != '\0')) )
		{
		switch (*a_p)
			{
			case '*':
				a_p++;
				l_meos = 0;
				while ( (*a_s != '\0') && (!(l_meos = s_ref(a_s++, a_p))) );
				if ( l_meos )
					{
					while (*a_s) a_s++;
					while (*a_p) a_p++;
					}
				break;

			case '%':
				a_p++;
				a_s++;
				break;

			default:
				if ( *a_s == *a_p )
					{ a_s++; a_p++; }
				else	l_done = 1;
			}
		}

	while ( *a_p == '*' )
		a_p++;

	return	( (*a_s == '\0') && (*a_p == '\0') );
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
	CHECK(  __util$pattern_match("", "*"),                      "a lone star matches the empty string");
	CHECK(  __util$pattern_match("", ""),                       "empty matches empty");
	CHECK( !__util$pattern_match("a", ""),                      "an empty pattern rejects a non-empty string");
	CHECK(  __util$pattern_match("abc", "a**c"),                "adjacent stars collapse");

	/* Fuzz against the former recursive matcher: strings over {a,b} up to 10,
	** patterns over {a,b,*,%} up to 8 - small enough for the reference to stay
	** fast, rich enough to cover every branch.
	*/
	srand(20260814);
	for ( l_i = 0, l_bad = 0; l_i < 500000; l_i++ )
		{
		l_sl = rand() % 11;
		l_pl = rand() % 9;
		for ( l_k = 0; l_k < l_sl; l_k++) l_s[l_k] = "ab"[rand() % 2];
		l_s[l_sl] = '\0';
		for ( l_k = 0; l_k < l_pl; l_k++) l_p[l_k] = "ab*%"[rand() % 4];
		l_p[l_pl] = '\0';

		if ( !!__util$pattern_match(l_s, l_p) != !!s_ref(l_s, l_p) )
			l_bad++;
		}
	CHECK( 0 == l_bad, "500000 random cases agree with the recursive reference (%ld diverged)", l_bad);

	/* The pathological pattern: several stars over a long run of one octet took
	** the recursive matcher seconds; the iterative one is bounded by |s|*|p|.
	*/
	memset(l_hay, 'a', 200);
	l_hay[200] = '\0';
	clock_gettime(CLOCK_MONOTONIC, &l_t0);
	CHECK( !__util$pattern_match(l_hay, "a*a*a*a*a*b"), "the pathological pattern still mismatches");
	clock_gettime(CLOCK_MONOTONIC, &l_t1);
	l_dt = (l_t1.tv_sec - l_t0.tv_sec) + (l_t1.tv_nsec - l_t0.tv_nsec) * 1e-9;
	CHECK( l_dt < 0.05, "...and comes back at once, not in seconds (%.6f s)", l_dt);

	TEST_DONE();
}
