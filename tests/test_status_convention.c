/*
**++
**
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  Locks in the OpenVMS condition-value convention across the time
**	facility.  Every status must be judged by its low bit (odd = success,
**	even = failure) - NOT by comparison to a particular code.  The test
**	fails (non-zero exit) if any success code turns even, any failure code
**	turns odd, or bintim/asctim/CLI$K_TIME stop honouring 1 & status.  It also
**	locks the condition-value layout itself - $FAC/$MSG/$SEV must invert
**	$DEFFAC/$DEFMSG/$DEFSEV field for field.
**
**  MODULE:  test_status_convention.c
**
**  AUTHORS: DN
**
**  CREATION DATE:  01-AUG-2026
**
**--
*/

#define __MODULE__ "TST$"

#include <stdio.h>
#include <string.h>

#include "utility_routines.h"
#include "starlet.h"
#include "cli_routines.h"

#include "test_harness.h"
/*
 *  Group 1 - the severity of the codes themselves.  A success code must be odd,
 *  a failure code must be even, so that "1 & status" classifies them correctly.
 */
static void t_code_severity (void)
{
	printf("[1] condition-code severity (odd=success, even=failure)\n");

	CHECK( 1 & STS$K_SUCCESS,  "STS$K_SUCCESS must be odd");
	CHECK(   1 & STS$K_INFO,   "STS$K_INFO must be odd");

	CHECK( !(1 & STS$K_WARN),  "STS$K_WARN must be even");
	CHECK( !(1 & STS$K_ERROR), "STS$K_ERROR must be even");
	CHECK( !(1 & STS$K_FATAL), "STS$K_FATAL must be even");

	CHECK( !(1 & FAO$K_IVTIME), "FAO$K_IVTIME must be even (was 7 - odd - and read as success)");
	CHECK( !(1 & FAO$K_BUFOVF), "FAO$K_BUFOVF must be even (was 5 - odd)");
	CHECK( !(1 & FAO$K_BADDIR), "FAO$K_BADDIR must be even");
	CHECK( !(1 & FAO$K_NEGTIM), "FAO$K_NEGTIM must be even");
}

/*
 *  Group 2 - __util$bintim() honours 1 & status on valid and on garbage input.
 */
static void t_bintim (void)
{
	static const char *l_good [] = {
		"15-OCT-2018 15:17:13", "15-OCT-2018 15:17:13.00",
		"15-OCT-2018", "0 00:00:10", "1 12:00:00" };
	static const char *l_bad [] = {
		"", "not-a-date", "garbage123", "31-XYZ-2018",
		"15-OCT-2018 99:99:99" };
	fao_time_t	l_t;
	unsigned	i;

	printf("[2] __util$bintim() by 1 & status\n");

	for ( i = 0; i < sizeof(l_good)/sizeof(l_good[0]); i++ )
		CHECK( 1 & __util$bintim(l_good[i], &l_t), "bintim('%s') must succeed", l_good[i]);

	for ( i = 0; i < sizeof(l_bad)/sizeof(l_bad[0]); i++ )
		CHECK( !(1 & __util$bintim(l_bad[i], &l_t)), "bintim('%s') must fail", l_bad[i]);
}

/*
 *  Group 3 - __util$asctim() succeeds and round-trips a known time.
 */
static void t_asctim (void)
{
	fao_time_t	l_t;
	char		l_buf[64];
	unsigned short	l_n;
	fao_desc_t	l_d;
	int		l_st;

	printf("[3] __util$asctim() round-trip\n");

	__util$bintim("15-OCT-2018 15:17:13.00", &l_t);
	l_d  = (fao_desc_t) $FAO_DESC(l_buf);
	l_st = __util$asctim(&l_n, &l_d, &l_t, 0);

	CHECK( 1 & l_st, "asctim must succeed");

	l_buf[l_n] = '\0';
	CHECK( 0 == strncmp(l_buf, "15-OCT-2018 15:17:13", 19),
		"asctim round-trip must yield '15-OCT-2018 15:17:13...', got '%s'", l_buf);
}

/*
 *  Group 4 - CLI$K_TIME / CLI$K_MAC validation honours 1 & status through the parser.
 */
static CLI_PQDESC s_quals [] = {
	{ .name = {$ASCINI("when")}, .type = CLI$K_TIME, .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("mac")},  .type = CLI$K_MAC,  .pn = CLI$K_QUAL },
	{ .name = {0} }
};
static CLI_VERB s_cmds [] = {
	{ .name = {$ASCINI("run")}, .quals = s_quals },
	{ .name = {0} }
};

static int s_parse (char *a_val)
{
	void	*l_ctx = NULL;
	char	*l_argv[2] = { "run", a_val };
	int	l_st;

	l_st = __cli$parse(s_cmds, 0, 2, l_argv, &l_ctx);
	__cli$cleanup(l_ctx);
	return	l_st;
}

static void t_cli_types (void)
{
	printf("[4] CLI$K_TIME / CLI$K_MAC by 1 & status\n");

	CHECK(   1 & s_parse("/when=15-OCT-2018 15:17:13"), "CLI$K_TIME valid must parse");
	CHECK( !(1 & s_parse("/when=nonsense")),            "CLI$K_TIME garbage must reject");

	CHECK(   1 & s_parse("/mac=aa:bb:cc:dd:ee:ff"),     "CLI$K_MAC valid must parse");
	CHECK( !(1 & s_parse("/mac=zz:bb:cc:dd:ee:ff")),    "CLI$K_MAC garbage must reject");
}

/*
 *  Group 5 - the condition-value layout: | facility 16 | message number 13 | severity 3 |.
 *  $FAC/$MSG/$SEV must be the exact inverse of $DEFFAC/$DEFMSG/$DEFSEV, each field
 *  extracted free of its neighbours.  $MSG() used to omit the 13-bit mask, so for a
 *  non-zero facility it handed back <facility> << 13 | <number>.
 */
static void t_condition_fields (void)
{
	static const struct { unsigned fac, msg, sev; } l_tst [] = {
		{ 0x0000,      0, STS$K_WARN    },	/* every field at its floor	*/
		{ 0x00AB,      5, STS$K_INFO    },	/* the case that exposed $MSG()	*/
		{ 0x0001,      1, STS$K_SUCCESS },
		{ 0x7FFF,   0x7B, STS$K_ERROR   },
		{ 0xFFFF, 0x1FFF, STS$K_FATAL   }	/* every field at its ceiling	*/
	};
	unsigned	l_code, l_i;

	printf("[5] $FAC/$MSG/$SEV invert $DEFFAC/$DEFMSG/$DEFSEV\n");

	for ( l_i = 0; l_i < $ARRSZ(l_tst); l_i++ )
		{
		l_code = $DEFFAC(l_tst[l_i].fac) | $DEFMSG(l_tst[l_i].msg) | $DEFSEV(l_tst[l_i].sev);

		CHECK( $FAC(l_code) == l_tst[l_i].fac, "$FAC() must return the facility alone");
		CHECK( $MSG(l_code) == l_tst[l_i].msg, "$MSG() must return the message number alone");
		CHECK( $SEV(l_code) == l_tst[l_i].sev, "$SEV() must return the severity alone");
		}

	/* The fields must not overlap: each one alone leaves the other two at zero */
	CHECK( $MSG($DEFFAC(0x0ABU)) == 0, "the facility must not reach into the message number");
	CHECK( $SEV($DEFMSG(0x1FFFU)) == 0, "the message number must not reach into the severity");
	CHECK( $FAC($DEFMSG(0x1FFFU) | $DEFSEV(STS$K_FATAL)) == 0, "neither must reach into the facility");

	/* $STS_ERROR() judges the severity field, not the whole value */
	CHECK(   $STS_ERROR($DEFFAC(0x0ABU) | $DEFMSG(5) | $DEFSEV(STS$K_ERROR)),  "ERROR must be a failure");
	CHECK(   $STS_ERROR($DEFFAC(0x0ABU) | $DEFMSG(5) | $DEFSEV(STS$K_FATAL)),  "FATAL must be a failure");
	CHECK( !($STS_ERROR($DEFFAC(0x0ABU) | $DEFMSG(5) | $DEFSEV(STS$K_INFO))),  "INFO must not be a failure");
	CHECK( !($STS_ERROR($DEFFAC(0x0ABU) | $DEFMSG(5) | $DEFSEV(STS$K_WARN))),  "WARN must not be a failure");
}

int main (void)
{
	printf("== status-convention regression ==\n");

	t_code_severity();
	t_bintim();
	t_asctim();
	t_cli_types();
	t_condition_fields();

	TEST_DONE();
}
