/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  The small buffer and string helpers of utility_routines.h (bzero, fill, movc5,
**	strelem, str2asc, strcat_rx, collapse, uncomment, mask2sym/val2sym, isinrange2, the
**	timespec helpers, $ILE3INI, the CRC-32 check value).  Buffers that must not be overrun
**	are malloc()ed to the exact size, so AddressSanitizer reports a stray octet.
**
**  AUTHOR:  StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  31-OCT-2026
**
**  MODULE:  test_strings.c
**--
*/
#define __MODULE__ "STRT$"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef	_WIN32
#include <unistd.h>
#define	TEST_WATCHDOG(sec)	alarm(sec)	/* a fill() which loops forever ends the test with SIGALRM */
#else
#define	TEST_WATCHDOG(sec)
#endif
#include "test_harness.h"
#include "utility_routines.h"

static void s_bzero_iszero (void)
{
unsigned char	*l_buf = malloc(96);
int	l_off, l_len, l_i, l_bad = 0, l_badz = 0;

	for ( l_off = 0; l_off < 8; l_off++ )
		for ( l_len = 0; l_len <= 41; l_len++ )
			{
			memset(l_buf, 0xFF, 96);
			__util$bzero(l_buf + 8 + l_off, (size_t) l_len);

			for ( l_i = 0; l_i < 96; l_i++ )
				if ( (l_buf[l_i] == 0) != ((l_i >= 8 + l_off) && (l_i < 8 + l_off + l_len)) )
					l_bad++;
			}
	CHECK( 0 == l_bad, "bzero clears exactly the requested octets, at every alignment and length (%d wrong)", l_bad);

	memset(l_buf, 0, 96);
	for ( l_off = 0; l_off < 8; l_off++ )
		for ( l_len = 0; l_len <= 41; l_len++ )
			{
			if ( !(STS$K_SUCCESS == __util$iszero(l_buf + l_off, l_len)) )
				l_badz++;

			for ( l_i = 0; l_i < l_len; l_i++ )		/* a single non-zero octet anywhere is seen */
				{
				l_buf[l_off + l_i] = 1;
				if ( STS$K_WARN != __util$iszero(l_buf + l_off, l_len) )
					l_badz++;
				l_buf[l_off + l_i] = 0;
				}
			}
	CHECK( 0 == l_badz, "iszero sees a lone non-zero octet at any position, any alignment, any length (%d wrong)", l_badz);

	/* The tail of 4..7 octets was skipped where long is 4 bytes: octet 4..7 of an 8..15 byte buffer */
	memset(l_buf, 0, 96);
	l_buf[6] = 1;
	CHECK( STS$K_WARN == __util$iszero(l_buf, 7), "iszero: the 7th octet of 7 is looked at");
	CHECK( STS$K_WARN == __util$iszero(l_buf, 15), "iszero: the 7th octet of 15 is looked at");
	CHECK( STS$K_SUCCESS == __util$iszero(l_buf, 6), "iszero: ...but not past the length");
	CHECK( STS$K_SUCCESS == __util$iszero(NULL, 10), "iszero: a NULL buffer has nothing to see");
	CHECK( STS$K_SUCCESS == __util$bzero(NULL, 0), "bzero: NULL and 0 is a success");
	CHECK( !(1 & __util$bzero(NULL, 4)), "bzero: NULL and 4 is an error");

	free(l_buf);
}

static void s_fill_movc5 (void)
{
char	l_dst [16];
char	l_src [4] = "abc";
unsigned short	l_srclen, l_dstlen;
unsigned char	l_fill = '.';

	memset(l_dst, 'x', sizeof(l_dst));
	CHECK( 1 & __util$fill(l_dst, 8, l_src, 3), "fill succeeds");
	CHECK( 0 == memcmp(l_dst, "abcabcab", 8) && l_dst[8] == 'x', "fill repeats the pattern and cuts the last copy");

	TEST_WATCHDOG(5);		/* the old loop never ended */
	CHECK( !(1 & __util$fill(l_dst, 8, l_src, 0)), "fill with a pattern of size 0 is an error (and returns)");
	CHECK( !(1 & __util$fill(l_dst, 8, NULL, 3)), "fill with a NULL pattern is an error");
	CHECK( !(1 & __util$fill(l_dst, 8, l_src, -1)), "fill with a negative pattern size is an error");
	CHECK( (1 & __util$fill(l_dst, 0, l_src, 3)), "fill of nothing is a success");
	TEST_WATCHDOG(0);

	/* movc5: min(srclen, dstlen) octets are copied, the rest of the destination is filled */
	memset(l_dst, 'x', sizeof(l_dst));
	l_srclen = 3;
	l_dstlen = 8;
	CHECK( STS$K_SUCCESS == __util$movc5(&l_srclen, l_src, &l_fill, &l_dstlen, l_dst), "movc5 succeeds");
	CHECK( 0 == memcmp(l_dst, "abc.....", 8) && l_dst[8] == 'x', "movc5: copy 3, fill 5, not a byte more");

	memset(l_dst, 'x', sizeof(l_dst));
	l_srclen = 3;
	l_dstlen = 2;
	__util$movc5(&l_srclen, l_src, &l_fill, &l_dstlen, l_dst);
	CHECK( 0 == memcmp(l_dst, "abx", 3), "movc5: a source longer than the destination is cut");

	memset(l_dst, 'x', sizeof(l_dst));
	l_srclen = 0;
	l_dstlen = 4;
	__util$movc5(&l_srclen, l_src, &l_fill, &l_dstlen, l_dst);
	CHECK( 0 == memcmp(l_dst, "....x", 5), "movc5: an empty source fills it all");

	l_srclen = 2;
	l_dstlen = 4;
	__util$movc5(&l_srclen, l_src, NULL, &l_dstlen, l_dst);
	CHECK( 0 == memcmp(l_dst, "ab\0\0", 4), "movc5: a NULL fill address means zeros");

	strcpy(l_dst, "0123456789");		/* overlapping areas */
	l_srclen = 6;
	l_dstlen = 8;
	__util$movc5(&l_srclen, l_dst, &l_fill, &l_dstlen, l_dst + 2);
	CHECK( 0 == memcmp(l_dst + 2, "012345..", 8), "movc5: overlapping source and destination");
}

static void s_strelem_str2asc (void)
{
ASC	l_src, l_dst;
static const struct	{ const char *list; int idx; const char *elem; }	l_v [] = {
		{ "a,bb,,ccc", 0, "a" },	{ "a,bb,,ccc", 1, "bb" },	{ "a,bb,,ccc", 2, "" },
		{ "a,bb,,ccc", 3, "ccc" },	{ "a,bb,,ccc", 4, "" },		{ "a,bb,,ccc", 7, "" },
		{ "a,bb,,ccc", -1, "" },	{ "one", 0, "one" },		{ "one", 1, "" },
		{ ",x,", 0, "" },		{ ",x,", 1, "x" },		{ "w,x,y,z", 3, "z" },
		{ "w,x,y,z", 2, "y" },		{ "", 0, "" },			{ "", 1, "" },
	};
int	l_i, l_bad = 0, l_len;

	for ( l_i = 0; l_i < (int) (sizeof(l_v)/sizeof(l_v[0])); l_i++ )
		{
		__util$str2asc(l_v[l_i].list, &l_src);
		memset(&l_dst, 0x55, sizeof(l_dst));
		l_len = __util$strelem(&l_src, ',', l_v[l_i].idx, &l_dst);

		if ( (l_len != (int) strlen(l_v[l_i].elem)) || (l_dst.len != l_len) || strcmp(l_dst.sts, l_v[l_i].elem) )
			{
			l_bad++;
			printf("  strelem('%s', %d) -> %d '%s'\n", l_v[l_i].list, l_v[l_i].idx, l_len, l_dst.sts);
			}
		}
	CHECK( 0 == l_bad, "strelem returns the right element (or nothing) for every index (%d wrong)", l_bad);

	/* str2asc: 254 octets and the NUL fill the container, a longer text is cut */
	{
	struct	{ ASC a; unsigned char guard [8]; } l_s;
	char	l_text [400];

	memset(l_text, 'q', sizeof(l_text));

	for ( l_i = 250; l_i <= 300; l_i += 1 )
		{
		memset(&l_s, 0xA5, sizeof(l_s));
		l_text[l_i] = '\0';
		l_len = __util$str2asc(l_text, &l_s.a);
		l_text[l_i] = 'q';

		if ( (l_len != ((l_i < 254) ? l_i : 254)) || (l_s.a.len != l_len) || (l_s.a.sts[l_len] != '\0') || (0xA5 != l_s.guard[0]) )
			l_bad++;
		}
	CHECK( 0 == l_bad, "str2asc of 250...300 octets: cut at 254, NUL kept, nothing written past the ASC (%d wrong)", l_bad);

	l_text[255] = '\0';
	memset(&l_s, 0xA5, sizeof(l_s));
	CHECK( 254 == __util$str2asc(l_text, &l_s.a), "str2asc of a 255-octet text stores 254");
	CHECK( (0 == l_s.a.sts[254]) && (0xA5 == l_s.guard[0]), "...the NUL is sts[254], the octet after the container is intact");
	}
}

static void s_strcat_collapse (void)
{
char	*l_dst;
int	l_len, l_sts;

	/* Pieces which fill the area up to its last octet but one: success */
	l_dst = malloc(8);
	l_sts = __util$strcat_rx(l_dst, 8, &l_len, "abc", 3, "defg", 4, NULL);
	CHECK( (1 & l_sts) && (7 == l_len) && (0 == memcmp(l_dst, "abcdefg\0", 8)), "strcat_rx: 3 + 4 octets and the NUL fill 8 exactly: success, len 7");

	/* One octet more does not fit - and nothing is written at dst[8] (ASan) */
	l_sts = __util$strcat_rx(l_dst, 8, &l_len, "abc", 3, "defgh", 5, NULL);
	CHECK( !(1 & l_sts) && (3 == l_len) && (0 == strcmp(l_dst, "abc")), "strcat_rx: 3 + 5 do not fit in 8: error, what fitted is kept and NUL-terminated");

	l_sts = __util$strcat_rx(l_dst, 8, &l_len, "abcdefgh", 8, NULL);
	CHECK( !(1 & l_sts) && (0 == l_len) && (0 == l_dst[0]), "strcat_rx: 8 octets alone do not fit: error, empty result");

	l_sts = __util$strcat_rx(l_dst, 8, &l_len, "ab", 0, "cd", 2, NULL);
	CHECK( (1 & l_sts) && (2 == l_len) && !strcmp(l_dst, "cd"), "strcat_rx: a zero length skips the piece");

	l_sts = __util$strcat_rx(l_dst, 8, &l_len, NULL);
	CHECK( (1 & l_sts) && (0 == l_len) && (0 == l_dst[0]), "strcat_rx: an empty list is an empty string");
	free(l_dst);

	l_dst = malloc(1);
	l_sts = __util$strcat_rx(l_dst, 1, &l_len, NULL);
	CHECK( (1 & l_sts) && (0 == l_len) && (0 == l_dst[0]), "strcat_rx: an area of one octet takes the NUL only");
	l_sts = __util$strcat_rx(l_dst, 1, &l_len, "a", 1, NULL);
	CHECK( !(1 & l_sts) && (0 == l_len) && (0 == l_dst[0]), "strcat_rx: ...and not a piece");
	free(l_dst);

	CHECK( !(1 & __util$strcat_rx(&l_sts, 0, &l_len, NULL)), "strcat_rx: an area of 0 octets is an error and is not written");

	/* collapse: no room for the NUL when nothing is removed */
	l_dst = malloc(5);
	memcpy(l_dst, "abcde", 5);
	CHECK( 5 == __util$collapse(l_dst, 5) && !memcmp(l_dst, "abcde", 5), "collapse: nothing to remove in a buffer without room for the NUL - no write at [5]");
	free(l_dst);

	l_dst = malloc(8);
	memcpy(l_dst, " a b\tc\n", 7);
	l_dst[7] = 'Z';
	CHECK( 3 == __util$collapse(l_dst, 7), "collapse: 3 octets remain of ' a b<TAB>c<LF>'");
	CHECK( !strcmp(l_dst, "abc") && (l_dst[7] == 'Z'), "...NUL-terminated after them, the byte past the buffer untouched");
	free(l_dst);

	l_dst = malloc(3);
	memcpy(l_dst, "   ", 3);
	CHECK( 0 == __util$collapse(l_dst, 3) && !l_dst[0], "collapse: all white space leaves the empty string");
	free(l_dst);
}

static void s_uncomment (void)
{
static const struct	{ const char *in; const char *out; int len; }	l_v [] = {
		{ "abc ! comment",		"abc ",			4 },
		{ "!all comment",		"",			0 },
		{ "no comment",			"no comment",		10 },
		{ "x=\"a!b\" ! c",		"x=\"a!b\" ",		8 },
		{ "x='a!b'",			"x='a!b'",		7 },
		{ "x='a!b' !c",		"x='a!b' ",		8 },
		{ "x=\"it's!\" !c",		"x=\"it's!\" ",		10 },
		{ "x='say \"!\"' !c",		"x='say \"!\"' ",	12 },
		{ "x=\"a!b",			"x=\"a!b",		6 },
		{ "a!b!c",			"a",			1 },
		{ "\"!\"\"!\"!z",		"\"!\"\"!\"",		6 },
	};
char	l_buf [64];
int	l_i, l_len, l_bad = 0;

	for ( l_i = 0; l_i < (int) (sizeof(l_v)/sizeof(l_v[0])); l_i++ )
		{
		strcpy(l_buf, l_v[l_i].in);
		l_len = __util$uncomment(l_buf, (int) strlen(l_buf), '!');

		if ( (l_len != l_v[l_i].len) || strcmp(l_buf, l_v[l_i].out) )
			{
			l_bad++;
			printf("  uncomment('%s') -> %d '%s'\n", l_v[l_i].in, l_len, l_buf);
			}
		}
	CHECK( 0 == l_bad, "uncomment cuts at the first marker outside the quotes (%d wrong)", l_bad);
	CHECK( 0 == __util$uncomment(NULL, 5, '!') && 0 == __util$uncomment(l_buf, 0, '!'), "uncomment: NULL and zero length give 0");
}

static void s_mask2sym (void)
{
static const SYM_REC	l_tbl [] = {
		{ 1, 2, "AA" }, { 2, 2, "BB" }, { 4, 2, "CC" }, { 8, 5, "DDDDD" }, { 0, 0, NULL },
	};
char	*l_out;
int	l_sz, l_len, l_bad = 0;
static const char	*l_exp [] = {
		"", "", "", "AA", "AA", "AA", "AA|BB", "AA|BB", "AA|BB", "AA|BB|CC", "AA|BB|CC",
	};

	/* mask 7 -> "AA|BB|CC" needs 9 octets with the NUL; every smaller area must come out cut, never overrun */
	for ( l_sz = 1; l_sz <= 10; l_sz++ )
		{
		l_out = malloc((size_t) l_sz);			/* exact: ASan sees a write past the end */
		l_len = __util$mask2sym(7, l_tbl, l_out, l_sz);

		if ( (l_len != (int) strlen(l_exp[l_sz])) || strcmp(l_out, l_exp[l_sz]) )
			{
			l_bad++;
			printf("  mask2sym(7, outsz %d) -> %d '%s'\n", l_sz, l_len, l_out);
			}
		free(l_out);
		}
	CHECK( 0 == l_bad, "mask2sym counts the separator and the NUL: the text is cut but fits the area (%d wrong)", l_bad);
	CHECK( 0 == __util$mask2sym(7, l_tbl, NULL, 0), "mask2sym: an area of 0 octets is not written");

	l_out = malloc(4);
	CHECK( 0 == __util$val2sym(3, l_tbl, l_out, 0) && 2 == __util$val2sym(2, l_tbl, l_out, 4) && !strcmp(l_out, "BB"), "val2sym: the value found, none for the unknown");
	free(l_out);
}

static void s_numbers_times (void)
{
struct timespec	l_a, l_b, l_r;

	CHECK( __util$isinrange2(5, 5, 10) && __util$isinrange2(10, 5, 10), "isinrange2 includes both borders");
	CHECK( __util$isinrange2(7, 5, 10) && !__util$isinrange2(4, 5, 10) && !__util$isinrange2(11, 5, 10), "isinrange2: inside yes, outside no");
	CHECK( __util$isinrange2(5, 10, 5) && __util$isinrange2(10, 10, 5) && __util$isinrange2(7, 10, 5), "isinrange2: the borders may be given in either order");
	CHECK( __util$isinrange2(3, 3, 3) && !__util$isinrange2(2, 3, 3), "isinrange2: a range of one value");

	/* sub_time: normalised */
	l_a.tv_sec = 5;	l_a.tv_nsec = 100;
	l_b.tv_sec = 3;	l_b.tv_nsec = 200;
	__util$sub_time(&l_a, &l_b, &l_r);
	CHECK( (1 == l_r.tv_sec) && (999999900 == l_r.tv_nsec), "sub_time borrows a second: 5.000000100 - 3.000000200 = 1.999999900 (got %ld.%09ld)", (long) l_r.tv_sec, l_r.tv_nsec);

	l_a.tv_sec = 5;	l_a.tv_nsec = 500000000;
	l_b.tv_sec = 3;	l_b.tv_nsec = 200000000;
	__util$sub_time(&l_a, &l_b, &l_r);
	CHECK( (2 == l_r.tv_sec) && (300000000 == l_r.tv_nsec), "sub_time without a borrow: 2.300000000 (got %ld.%09ld)", (long) l_r.tv_sec, l_r.tv_nsec);

	l_a.tv_sec = 3;	l_a.tv_nsec = 0;
	l_b.tv_sec = 3;	l_b.tv_nsec = 1;
	__util$sub_time(&l_a, &l_b, &l_r);
	CHECK( (-1 == l_r.tv_sec) && (999999999 == l_r.tv_nsec), "sub_time of a negative difference is -1 + 0.999999999 (got %ld.%09ld)", (long) l_r.tv_sec, l_r.tv_nsec);

	/* cmp_time: the sign of (time1 - time2) */
	l_a.tv_sec = 10;	l_a.tv_nsec = 5;
	l_b.tv_sec = 10;	l_b.tv_nsec = 5;
	CHECK( 0 == __util$cmp_time(&l_a, &l_b), "cmp_time: equal is 0");
	l_b.tv_nsec = 6;
	CHECK( 0 > __util$cmp_time(&l_a, &l_b), "cmp_time: time1 < time2 (nanoseconds) is negative");
	CHECK( 0 < __util$cmp_time(&l_b, &l_a), "cmp_time: time1 > time2 is positive");
	l_b.tv_sec = 11;	l_b.tv_nsec = 0;
	CHECK( 0 > __util$cmp_time(&l_a, &l_b) && 0 < __util$cmp_time(&l_b, &l_a), "cmp_time: seconds decide first");

	if ( sizeof(time_t) > 4 )
		{
		l_a.tv_sec = (time_t) 0x80000000LL;	l_a.tv_nsec = 0;	/* INT_MAX + 1 seconds apart */
		l_b.tv_sec = 0;				l_b.tv_nsec = 0;
		CHECK( 0 < __util$cmp_time(&l_a, &l_b) && 0 > __util$cmp_time(&l_b, &l_a), "cmp_time: a difference of more than INT_MAX seconds keeps its sign");
		l_a.tv_sec = (time_t) 0x100000000LL;
		CHECK( 0 < __util$cmp_time(&l_a, &l_b), "cmp_time: a difference of 2^32 seconds (the old (int) cast gave 0) is positive");
		}
}

static void s_misc (void)
{
ILE3	l_ile;
unsigned short	l_ret = 0;
int	l_val = 0;

	/* $ILE3INI with ordinary names for its arguments; the layout is code first, then the length */
	$ILE3INI(l_ile, 0x1234, 4, &l_val, &l_ret);
	CHECK( (0x1234 == l_ile.code) && (4 == l_ile.len) && ((char *) &l_val == l_ile.ptr) && (&l_ret == l_ile.retlen), "$ILE3INI fills the element");
	CHECK( (0 == offsetof(ILE3, code)) && (2 == offsetof(ILE3, len)), "ILE3: the code word comes first (not the VMS layout)");

	{
	unsigned short	l_code = 7, l_len = 8, *l_retlen = &l_ret;
	char	*l_ptr = (char *) &l_val;

	$ILE3INI(l_ile, l_code, l_len, l_ptr, l_retlen);	/* the arguments are named like the members */
	CHECK( (7 == l_ile.code) && (8 == l_ile.len) && (l_ptr == l_ile.ptr) && (l_retlen == l_ile.retlen), "$ILE3INI with arguments named code, len, ptr, retlen");
	}

	/* The CRC is the IEEE CRC-32, whatever the name says */
	CHECK( 0xCBF43926U == __util$crc32c(0, "123456789", 9), "__util$crc32c is the IEEE 802.3 CRC-32: check value 0xCBF43926");
}

static void s_hex2bin (void)
{
unsigned char	l_o [8];
static const struct	{ const char *in; int n; }	l_bad [] = {
		{ "abc", 3 }, { "a", 1 }, { "zz", 2 }, { "0g", 2 }, { "12 4", 4 }, { "0x12", 4 }, { "12\0034", 4 }, { "-1", 2 },
	};
int	l_i, l_n, l_changed = 0;

	memset(l_o, 0xAA, sizeof(l_o));
	l_n = __util$hex2bin("c0A80102", l_o, 8);
	CHECK( (4 == l_n) && (0xC0 == l_o[0]) && (0xA8 == l_o[1]) && (0x01 == l_o[2]) && (0x02 == l_o[3]) && (0xAA == l_o[4]), "hex2bin: both cases, 4 octets, nothing past them");

	memset(l_o, 0xAA, sizeof(l_o));
	l_n = __util$hex2bin("deadBEEF", l_o, 8);
	CHECK( (4 == l_n) && !memcmp(l_o, "\xde\xad\xbe\xef", 4), "hex2bin: deadBEEF");

	memset(l_o, 0xAA, sizeof(l_o));
	CHECK( 0 == __util$hex2bin("", l_o, 0) && (0xAA == l_o[0]), "hex2bin: an empty text gives 0 and writes nothing");

	/* The text need not end with a NUL: only <len> octets are looked at */
	memset(l_o, 0xAA, sizeof(l_o));
	CHECK( 1 == __util$hex2bin("12ZZ", l_o, 2) && (0x12 == l_o[0]) && (0xAA == l_o[1]), "hex2bin: only <len> octets are read");

	for ( l_i = 0; l_i < (int) (sizeof(l_bad)/sizeof(l_bad[0])); l_i++ )
		{
		memset(l_o, 0xAA, sizeof(l_o));
		l_n = __util$hex2bin(l_bad[l_i].in, l_o, (unsigned short) l_bad[l_i].n);

		if ( (-1 != l_n) || memcmp(l_o, "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA", 8) )
			{
			l_changed++;
			printf("  hex2bin('%s') -> %d\n", l_bad[l_i].in, l_n);
			}
		}
	CHECK( 0 == l_changed, "hex2bin: an odd length or a non-hex character gives -1 and writes nothing (%d wrong)", l_changed);
}

int main (void)
{
	printf("== strings and buffers regression ==\n");

	s_bzero_iszero();
	s_fill_movc5();
	s_strelem_str2asc();
	s_strcat_collapse();
	s_uncomment();
	s_mask2sym();
	s_numbers_times();
	s_misc();
	s_hex2bin();

	TEST_DONE();
}
