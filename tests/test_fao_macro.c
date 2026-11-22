/*
**++
**
**  FACILITY:  starlet - regression tests
**
**  ABSTRACT:	The $FAO() macro and __util$faoc(): the parameters written plain give the same text
**	as __util$fao with casts; none, one and sixteen parameters; a control string that wants more
**	than were given gets 0 and FAO$K_NOPRM; a pointer, a quadword, a negative longword, a time.
**	starlet.h is deliberately the FIRST include: it must work without any help.
**
**  MODULE:  test_fao_macro.c
**
**  AUTHORS: RRL
**
**  CREATION DATE:  22-NOV-2026
**
**--
*/

#define __MODULE__ "TST$"

#include "starlet.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "test_harness.h"

static char		g_buf [512];
static unsigned short	g_len;

#define	SAME(want)	( (g_len == strlen(want)) && !memcmp(g_buf, want, g_len) )

int	main (void)
{
fao_desc_t	l_dsc = $FAO_DESC(g_buf);
char		l_ref [512];
fao_desc_t	l_rdsc = $FAO_DESC(l_ref);
unsigned short	l_rlen = 0;
unsigned char	l_ip [4] = { 192, 168, 1, 10 };
int		l_sts;
uint32_t	l_u32 = 4000000000U;
int32_t		l_neg = -42;
int64_t		l_q = -5000000000LL;

	/* The example of the manual */
	l_sts = $FAO("!AZ has !UL file!%S", &g_len, &l_dsc, "BACKUP", 3);
	CHECK( (l_sts == STS$K_SUCCESS) && SAME("BACKUP has 3 files"), "plain parameters: %.*s", g_len, g_buf);

	/* No parameter at all */
	l_sts = $FAO("no directives!/", &g_len, &l_dsc);
	CHECK( (l_sts == STS$K_SUCCESS) && SAME("no directives\r\n"), "no parameters: %.*s", g_len, g_buf);

	/* The same text as __util$fao with the casts written by hand */
	l_sts = $FAO("peer !%I got !UL file!%S, mask !XL", &g_len, &l_dsc, l_ip, 2, 0xFF00);
	__util$fao("peer !%I got !UL file!%S, mask !XL", &l_rlen, &l_rdsc, (fao_prm_t)(uintptr_t) l_ip, (fao_prm_t) 2, (fao_prm_t) 0xFF00);
	CHECK( (l_sts == STS$K_SUCCESS) && (g_len == l_rlen) && !memcmp(g_buf, l_ref, g_len), "as __util$fao: %.*s", g_len, g_buf);
	CHECK( SAME("peer 192.168.1.10 got 2 files, mask 0000FF00"), "address directive: %.*s", g_len, g_buf);

	/* Widths: an unsigned longword above 2^31, a negative longword, a negative quadword */
	l_sts = $FAO("!UL !SL !SQ", &g_len, &l_dsc, l_u32, l_neg, l_q);
	CHECK( (l_sts == STS$K_SUCCESS) && SAME("4000000000 -42 -5000000000"), "widths: %.*s", g_len, g_buf);

	/* Sixteen parameters - the most FAO$_EACH takes */
	l_sts = $FAO("!UL!UL!UL!UL!UL!UL!UL!UL!UL!UL!UL!UL!UL!UL!UL!UL", &g_len, &l_dsc, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6);
	CHECK( (l_sts == STS$K_SUCCESS) && SAME("1234567890123456"), "sixteen: %.*s", g_len, g_buf);

	/* Fewer parameters than the control string wants: 0 and FAO$K_NOPRM, nothing read past the list */
	l_sts = $FAO("!UL and !UL", &g_len, &l_dsc, 7);
	CHECK( (l_sts == FAO$K_NOPRM) && SAME("7 and 0"), "too few: sts=%d %.*s", l_sts, g_len, g_buf);

	/* __util$faoc refuses a negative count */
	CHECK( __util$faoc("x", &g_len, &l_dsc, -1) == STS$K_ERROR, "negative count");

	TEST_DONE();
}
