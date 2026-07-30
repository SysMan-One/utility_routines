/*
**++
**
**  FACILITY:	STARLET - Portable analogues of OpenVMS system services
**
**  MODULE:	starlet.c
**
**  ABSTRACT:	Portable (ISO C, freestanding-friendly) implementations
**		of OpenVMS system services.  The module is intended to
**		accumulate further VMS analogues over time; currently
**		implemented:
**
**		  UTIL$FAO / UTIL$FAOL - SYS$FAO / SYS$FAOL,
**			Formatted ASCII Output
**		  UTIL$BINTIM          - SYS$BINTIM,
**			ASCII time string to binary time
**		  UTIL$GETTIM          - SYS$GETTIM,
**			current binary time
**		  UTIL$ASCTIM          - SYS$ASCTIM,
**			binary time to ASCII string
**		  UTIL$ADD_TIMES       - LIB$ADD_TIMES,
**			sum of two binary times
**		  UTIL$SUB_TIMES       - LIB$SUB_TIMES,
**			difference of two binary times
**
**		Supported FAO directives:
**
**		Strings:
**		  !AS  - string by descriptor (fao_desc_t *)
**		  !AZ  - zero-terminated string (char *)
**		  !AC  - counted string, first byte is the length
**		  !AD  - two parameters: length, address
**		  !AF  - as !AD, non-printables are replaced with '.'
**
**		Numeric (B - byte, W - word, L - longword, Q - quadword;
**		quadword parameter is passed BY REFERENCE as on OpenVMS):
**		  !UB !UW !UL !UQ - unsigned decimal
**		  !SB !SW !SL !SQ - signed   decimal
**		  !XB !XW !XL !XQ - hexadecimal, zero-filled to 2/4/8/16
**		  !OB !OW !OL !OQ - octal,       zero-filled to 3/6/11/22
**		  !ZB !ZW !ZL !ZQ - unsigned decimal, zero-filled
**
**		Control:
**		  !/  - CR+LF		!_  - horizontal tab
**		  !^  - form feed	!!  - literal '!'
**		  !%S - 's' if the last converted number != 1
**		  !%T - time,  hh:mm:ss.cc  (UTC + FAO$K_TZOFFSET)
**		  !%D - date,  dd-MMM-yyyy hh:mm:ss.cc
**			(parameter: 0 - current time, else time_t *)
**
**		Width / repetition (n - decimal number or '#' to take
**		the value from the parameter list):
**		  !nXL ...  - explicit output field width
**		  !n*c      - repeat character 'c' n times
**		  !n(...)   - repeat the directive group n times
**		  !n<...!>  - fixed-width field, blank-filled/truncated
**		  !n-       - reuse last n parameters
**		  !n+       - skip next n parameters
**
**		Semantics of overflow of an explicit width field follows
**		OpenVMS: decimal fields are filled with '*', hex/octal
**		fields are truncated on the left, strings are truncated
**		on the right.
**
**  DESIGN:	CRTL usage is reduced to the bare minimum:
**		  - no <stdio.h>, <string.h>, <ctype.h>;
**		  - character classification, copying, string length and
**		    all numeric/date conversions are private inlines;
**		  - date/time breakdown is computed by the module itself
**		    (proleptic Gregorian, days-from-civil inverse), the
**		    only external call is time() and it can be overridden
**		    with the FAO$GETTIME macro for freestanding builds;
**		  - the $LOG fallback is a no-op unless the UTIL$ macro
**		    is supplied by the includer.
**
**		Speed:
**		  - literal runs between directives are copied in bulk
**		    with a single bounds check;
**		  - output goes through raw pointers, no index math;
**		  - decimal conversion emits two digits per division
**		    via a 200-byte lookup table;
**		  - the !n<...!> sub-field shares the parameter state by
**		    reference, no context copying.
**
**  AUTHOR:	Claude (Anthropic) for D.Novikov
**
**  CREATION DATE:  24-JUL-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-06		27-JUL-2026	CLAUDE
**		avproto declaration style: routine locals declared at
**		the first position (no indent), nested block
**		declarations hoisted to the routine top; lookup tables
**		s_hexdef/s_octdef/s_mdays moved to module scope.
**		No functional changes.
**
**	X.01-05		27-JUL-2026	CLAUDE
**		Static routines renamed with the s_ prefix
**		(fao$$name -> s_fao$name), per the coding standard.
**		No functional changes.
**
**	X.01-04		24-JUL-2026	CLAUDE
**		UTIL$GETTIM, UTIL$ASCTIM (SYS$GETTIM/SYS$ASCTIM) and
**		UTIL$ADD_TIMES, UTIL$SUB_TIMES (LIB$ADD_TIMES/
**		LIB$SUB_TIMES) added; FAO$K_NEGTIM status added.
**
**	X.01-03		24-JUL-2026	CLAUDE
**		Module renamed fao.c -> starlet.c (facility STARLET),
**		to accumulate further portable OpenVMS system service
**		analogues.  No functional changes.
**
**	X.01-02		24-JUL-2026	CLAUDE
**		Routine prefix changed sys$ -> util$; UTIL$BINTIM added
**		(portable SYS$BINTIM: absolute/delta ASCII time to
**		binary); calendar conversions factored out into
**		S_FAO$DAYS_TO_CIVIL / S_FAO$DAYS_FROM_CIVIL.
**
**	X.01-01		24-JUL-2026	CLAUDE
**		Remove CRTL dependencies (stdio/string/ctype/localtime),
**		private date conversion, bulk literal copy, two-digit
**		decimal conversion, pointer-based output, shared
**		parameter state for sub-fields.
**
**	X.01-00		24-JUL-2026	CLAUDE
**		Initial version.
**
**--
*/

#define	__MODULE__	"STARLET"
#define	__IDENT__	"X.01-08"
#define	__REV__		"1.06"

#include	<stdarg.h>
#include	<stdint.h>
#include	<stddef.h>

#include	"starlet.h"

/*
**  Time source (fao_time_t itself is defined in fao.h).  A freestanding
**  build may supply its own with:
**	#define FAO$GETTIME()	(my_rtc_read())
**  otherwise time() is the single CRTL routine referenced by the module.
*/
#ifndef	FAO$GETTIME
#define	FAO$GETTIME()	time(NULL)
#endif

/*
**  Timezone offset in seconds applied to !%T/!%D (default UTC).
*/
#ifndef	FAO$K_TZOFFSET
#define	FAO$K_TZOFFSET	0
#endif

/*
**  Fallback of the $LOG macro, normally supplied by the UTIL$ module.
**  Deliberately a no-op to keep the module free of <stdio.h>; the error
**  is still reported to the caller via the FAO$K_BADDIR status.
*/
#ifndef	$LOG
#define	$LOG(sts, fmt, ...)	((void) 0)
#endif

/*
**  Module-private constants and inline primitives (CRTL replacements)
*/
#define	FAO$K_WIDTHAUTO	(-1)	/* No explicit width given		*/
#define	FAO$K_NUMBUF	32	/* Scratch buffer for numeric conversion*/
#define	FAO$K_FLDBUF	512	/* Scratch buffer for !n< > sub-fields	*/

#define	$FAO_ISDIGIT(c)	( ((unsigned char)((c) - '0')) < 10u )
#define	$FAO_ISPRINT(c)	( ((unsigned char)((c) - 0x20)) < 0x5Fu )

static	const char	s_hexdig [16]	= "0123456789ABCDEF";

/*
**  Two-digit decimal lookup: s_dig2[2*i], s_dig2[2*i+1] = tens, units.
*/
static	const char	s_dig2 [200]	=
	"00010203040506070809" "10111213141516171819"
	"20212223242526272829" "30313233343536373839"
	"40414243444546474849" "50515253545556575859"
	"60616263646566676869" "70717273747576777879"
	"80818283848586878889" "90919293949596979899";

static	const int8_t	s_hexdef [4]	= { 2, 4,  8, 16 };
static	const int8_t	s_octdef [4]	= { 3, 6, 11, 22 };

static	const unsigned char s_mdays [12] =
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static	const char	s_months [12][4] =
	{
	"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
	"JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
	};

/*
**++
**  S_FAO$COPY - private byte copy (memcpy replacement, non-overlapping).
**
**  INPUT:
**	a_dst	- destination address
**	a_src	- source address
**	a_len	- byte count
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	NONE
**--
*/
static	inline	void	s_fao$copy (char * restrict a_dst,
				const char * restrict a_src, size_t a_len)
{
	while ( a_len-- )
		*a_dst++ = *a_src++;
}

/*
**++
**  S_FAO$STRLEN - private string length (strlen replacement).
**
**  INPUT:
**	a_str	- address of the zero-terminated string
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	Length in bytes, terminator excluded
**--
*/
static	inline	size_t	s_fao$strlen (const char *a_str)
{
const char *l_ptr = a_str;

	while ( *l_ptr )
		l_ptr++;

	return	(size_t) (l_ptr - a_str);
}

/*
**  Shared parameter state.  Kept apart from the output state so that a
**  !n<...!> sub-field can reference it instead of copying the (large)
**  parameter cache back and forth.
*/
typedef	struct	__fao_prmstate
{
	const fao_prm_t	*prmlst;	/* $FAOL: explicit array	*/
	int		prmlstcnt;	/* $FAOL: its element count	*/

	va_list		*ap;		/* $FAO: source of parameters	*/
	int		prmfetched;	/* $FAO: elements in the cache	*/
	int		prmidx;		/* Index of the next parameter	*/

	int64_t		lastnum;	/* Last number, for !%S		*/
	fao_prm_t	prmcache [FAO$K_MAXPRM];
} fao_prmstate_t;

/*
**  FAO conversion context: raw output window plus a reference to the
**  shared parameter state and the sticky completion status.
*/
typedef	struct	__fao_ctx
{
	char		*optr;		/* Next free output byte	*/
	char		*oend;		/* Past the end of the buffer	*/
	char		*obase;		/* Start of the buffer		*/

	fao_prmstate_t	*prm;		/* Shared parameter state	*/
	int		sts;		/* Sticky completion status	*/
} fao_ctx_t;

/*
**++
**  S_FAO$GET_PRM - fetch the next parameter from the list.
**
**	For $FAOL the parameter is taken from the explicit array; out of
**	range access yields 0.  For $FAO the parameters are consumed from
**	the va_list on demand and cached, so that !n- (parameter reuse)
**	can be honoured.
**
**  INPUT:
**	a_prm	- address of the shared parameter state
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	Value of the parameter (machine word)
**--
*/
static	fao_prm_t	s_fao$get_prm	(fao_prmstate_t *a_prm)
{
fao_prm_t l_val = 0;

	if ( a_prm->prmlst != NULL )
		{
		if ( a_prm->prmidx < a_prm->prmlstcnt )
			l_val = a_prm->prmlst [a_prm->prmidx];

		a_prm->prmidx++;
		return	l_val;
		}

	/* $FAO: replay from the cache or fetch a new one from va_list */
	while ( (a_prm->prmidx >= a_prm->prmfetched)
		&& (a_prm->prmfetched < FAO$K_MAXPRM) )
		{
		a_prm->prmcache [a_prm->prmfetched++] =
			va_arg (*a_prm->ap, fao_prm_t);
		}

	if ( a_prm->prmidx < a_prm->prmfetched )
		l_val = a_prm->prmcache [a_prm->prmidx];

	a_prm->prmidx++;
	return	l_val;
}

/*
**++
**  S_FAO$PUT_CHR - emit a single byte into the output buffer.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_chr	- byte to be emitted
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	NONE.  On buffer overflow the sticky status of the context is
**	set to FAO$K_BUFOVF and the byte is discarded.
**--
*/
static	inline	void	s_fao$put_chr	(fao_ctx_t *a_ctx, char a_chr)
{
	if ( a_ctx->optr < a_ctx->oend )
		*a_ctx->optr++ = a_chr;
	else	a_ctx->sts = FAO$K_BUFOVF;
}

/*
**++
**  S_FAO$PUT_FILL - emit a run of identical bytes.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_chr	- fill byte
**	a_cnt	- run length
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	NONE
**--
*/
static	void	s_fao$put_fill	(fao_ctx_t *a_ctx, char a_chr, size_t a_cnt)
{
size_t	l_room = (size_t) (a_ctx->oend - a_ctx->optr);

	if ( a_cnt > l_room )
		{
		a_ctx->sts = FAO$K_BUFOVF;
		a_cnt = l_room;
		}

	while ( a_cnt-- )
		*a_ctx->optr++ = a_chr;
}

/*
**++
**  S_FAO$PUT_RAW - emit a byte string in bulk (single bounds check).
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_str	- address of the bytes
**	a_len	- byte count
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	NONE
**--
*/
static	inline	void	s_fao$put_raw	(fao_ctx_t *a_ctx,
				const char *a_str, size_t a_len)
{
size_t	l_room = (size_t) (a_ctx->oend - a_ctx->optr);

	if ( a_len > l_room )
		{
		a_ctx->sts = FAO$K_BUFOVF;
		a_len = l_room;
		}

	s_fao$copy (a_ctx->optr, a_str, a_len);
	a_ctx->optr += a_len;
}

/*
**++
**  S_FAO$PUT_STR - emit a byte string honouring the field width.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_str	- address of the string (may be not zero-terminated)
**	a_len	- string length in bytes
**	a_width	- explicit field width or FAO$K_WIDTHAUTO;
**		  the string is left-justified, blank-filled on the
**		  right, truncated on the right if it does not fit
**	a_dots	- non-zero: replace non-printable bytes with '.'
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	NONE
**--
*/
static	void	s_fao$put_str	(fao_ctx_t *a_ctx, const char *a_str,
				size_t a_len, int a_width, int a_dots)
{
size_t	l_len = a_len, l_i;

	if ( (a_width != FAO$K_WIDTHAUTO) && ((size_t) a_width < l_len) )
		l_len = (size_t) a_width;

	if ( ! a_dots )
		s_fao$put_raw (a_ctx, a_str, l_len);	/* Fast path	*/
	else
		for ( l_i = 0; l_i < l_len; l_i++ )
			s_fao$put_chr (a_ctx, $FAO_ISPRINT (a_str [l_i])
				? a_str [l_i] : '.');

	if ( (a_width != FAO$K_WIDTHAUTO) && ((size_t) a_width > l_len) )
		s_fao$put_fill (a_ctx, ' ', (size_t) a_width - l_len);
}

/*
**++
**  S_FAO$CVT_DEC - convert an unsigned value to decimal digits,
**  filling the scratch buffer backwards, two digits per division.
**
**  INPUT:
**	a_val	- the value
**	a_end	- address PAST the last byte of the scratch buffer
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	Address of the first (most significant) digit
**--
*/
static	inline	char	*s_fao$cvt_dec	(uint64_t a_val, char *a_end)
{
char	*l_ptr = a_end;

	while ( a_val >= 100 )
		{
		unsigned l_rem = (unsigned) (a_val % 100);

		a_val /= 100;
		*--l_ptr = s_dig2 [l_rem * 2 + 1];
		*--l_ptr = s_dig2 [l_rem * 2];
		}

	*--l_ptr = s_dig2 [a_val * 2 + 1];

	if ( a_val >= 10 )
		*--l_ptr = s_dig2 [a_val * 2];

	return	l_ptr;
}

/*
**++
**  S_FAO$PUT_NUM - convert a number and emit it into the output buffer.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_val	- the value; for the signed conversion it is expected
**		  to be already sign-extended to 64 bits
**	a_base	- radix: 8, 10, 16
**	a_signed- non-zero: value is signed (decimal only)
**	a_width	- explicit field width or FAO$K_WIDTHAUTO
**	a_defdig- default digit count with zero fill (hex/octal and !Z
**		  directives), 0 - natural width, blank fill
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	NONE.  Overflow of an explicit field: decimal - the field is
**	filled with '*', hex/octal - the value is truncated on the left
**	(OpenVMS semantics).
**--
*/
static	void	s_fao$put_num	(fao_ctx_t *a_ctx, uint64_t a_val,
				int a_base, int a_signed,
				int a_width, int a_defdig)
{
char	l_buf [FAO$K_NUMBUF];
char	*l_end = l_buf + FAO$K_NUMBUF, *l_ptr;
int	l_len, l_field;
uint64_t l_val = a_val;
int	l_neg = 0;

	a_ctx->prm->lastnum = (int64_t) a_val;

	if ( a_signed && ((int64_t) a_val < 0) )
		{
		l_neg = 1;
		l_val = (uint64_t) ( - (int64_t) a_val);
		}

	if ( a_base == 10 )
		l_ptr = s_fao$cvt_dec (l_val, l_end);
	else if ( a_base == 16 )
		{
		l_ptr = l_end;
		do	{
			*--l_ptr = s_hexdig [l_val & 0xF];
			l_val >>= 4;
			} while ( l_val );
		}
	else	{				/* Octal		*/
		l_ptr = l_end;
		do	{
			*--l_ptr = s_hexdig [l_val & 07];
			l_val >>= 3;
			} while ( l_val );
		}

	l_len = (int) (l_end - l_ptr);

	/* Zero fill up to the default digit count of the directive */
	while ( (l_len < a_defdig) && (l_ptr > l_buf + 1) )
		{
		*--l_ptr = '0';
		l_len++;
		}

	if ( l_neg && (l_ptr > l_buf) )
		{
		*--l_ptr = '-';
		l_len++;
		}

	l_field = (a_width == FAO$K_WIDTHAUTO) ? l_len : a_width;

	if ( l_len > l_field )
		{
		if ( a_base == 10 )
			{			/* Decimal: asterisk fill */
			s_fao$put_fill (a_ctx, '*', (size_t) l_field);
			return;
			}

		l_ptr += l_len - l_field;	/* Hex/octal: left truncate */
		l_len = l_field;
		}

	if ( l_len < l_field )			/* Left padding		*/
		s_fao$put_fill (a_ctx,
			(a_defdig && (a_width == FAO$K_WIDTHAUTO)) ? '0' : ' ',
			(size_t) (l_field - l_len));

	s_fao$put_raw (a_ctx, l_ptr, (size_t) l_len);
}

/*
**++
**  S_FAO$PUT_2D - emit exactly two decimal digits (00..99).
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_val	- value, 0..99
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	NONE
**--
*/
/*
**++
**  S_FAO$DAYS_TO_CIVIL - break a UNIX epoch day count into a civil date
**  (proleptic Gregorian calendar, inverse of days-from-civil).
**
**  INPUT:
**	a_days	- days since 01-JAN-1970 (may be negative)
**
**  OUTPUT:
**	*a_year	- year
**	*a_mon	- month, 1..12
**	*a_day	- day of month, 1..31
**
**  RETURN:
**	NONE
**--
*/
static	void	s_fao$days_to_civil (int64_t a_days, int64_t *a_year,
				unsigned *a_mon, unsigned *a_day)
{
int64_t	l_z = a_days + 719468;
int64_t	l_era = (l_z >= 0 ? l_z : l_z - 146096) / 146097;
unsigned l_doe = (unsigned) (l_z - l_era * 146097);
unsigned l_yoe = (l_doe - l_doe/1460 + l_doe/36524 - l_doe/146096) / 365;
int64_t	l_year = (int64_t) l_yoe + l_era * 400;
unsigned l_doy = l_doe - (365*l_yoe + l_yoe/4 - l_yoe/100);
unsigned l_mp = (5*l_doy + 2) / 153;
unsigned l_day = l_doy - (153*l_mp + 2)/5 + 1;
unsigned l_mon = l_mp < 10 ? l_mp + 3 : l_mp - 9;

	*a_year	= l_year + (l_mon <= 2);
	*a_mon	= l_mon;
	*a_day	= l_day;
}

/*
**++
**  S_FAO$DAYS_FROM_CIVIL - convert a civil date to a UNIX epoch day
**  count (proleptic Gregorian calendar).
**
**  INPUT:
**	a_year	- year
**	a_mon	- month, 1..12
**	a_day	- day of month, 1..31
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	Days since 01-JAN-1970 (may be negative)
**--
*/
static	int64_t	s_fao$days_from_civil (int64_t a_year, unsigned a_mon,
				unsigned a_day)
{
int64_t	l_year = a_year - (a_mon <= 2);
int64_t	l_era = (l_year >= 0 ? l_year : l_year - 399) / 400;
unsigned l_yoe = (unsigned) (l_year - l_era * 400);
unsigned l_doy = (153 * (a_mon + (a_mon > 2 ? (unsigned)-3 : 9)) + 2) / 5
			+ a_day - 1;
	unsigned l_doe = l_yoe * 365 + l_yoe/4 - l_yoe/100 + l_doy;

	return	l_era * 146097 + (int64_t) l_doe - 719468;
}

static	inline	void	s_fao$put_2d	(fao_ctx_t *a_ctx, unsigned a_val)
{
	s_fao$put_raw (a_ctx, &s_dig2 [a_val * 2], 2);
}

/*
**++
**  S_FAO$PUT_TIME - emit time (!%T) or date+time (!%D).
**
**	The civil date is derived from the UNIX epoch count by the
**	module itself (proleptic Gregorian calendar, inverse of the
**	days-from-civil algorithm) - no CRTL localtime/gmtime involved.
**	The result is UTC shifted by FAO$K_TZOFFSET seconds.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_prm	- FAO parameter: 0 - current time, else time_t *
**	a_date	- non-zero: !%D (dd-MMM-yyyy hh:mm:ss.cc),
**		  zero: !%T (hh:mm:ss.cc)
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	NONE
**--
*/
static	void	s_fao$put_time	(fao_ctx_t *a_ctx, fao_prm_t a_prm, int a_date)
{
int64_t	l_now, l_days, l_secs, l_year;
unsigned l_hh, l_mm, l_ss, l_mon, l_day;

	if ( a_prm )
		l_now = (int64_t) * (const fao_time_t *) a_prm;
	else	l_now = (int64_t) FAO$GETTIME ();

	l_now += FAO$K_TZOFFSET;

	l_days = l_now / 86400;
	l_secs = l_now % 86400;

	if ( l_secs < 0 )
		{
		l_secs += 86400;
		l_days -= 1;
		}

	l_hh = (unsigned) (l_secs / 3600);
	l_mm = (unsigned) ((l_secs / 60) % 60);
	l_ss = (unsigned) (l_secs % 60);

	if ( a_date )
		{
		s_fao$days_to_civil (l_days, &l_year, &l_mon, &l_day);

		if ( l_day < 10 )
			s_fao$put_chr (a_ctx, ' ');
		else	s_fao$put_chr (a_ctx, (char)('0' + l_day / 10));

		s_fao$put_chr (a_ctx, (char)('0' + l_day % 10));
		s_fao$put_chr (a_ctx, '-');
		s_fao$put_raw (a_ctx, s_months [l_mon - 1], 3);
		s_fao$put_chr (a_ctx, '-');
		s_fao$put_2d  (a_ctx, (unsigned) (l_year / 100));
		s_fao$put_2d  (a_ctx, (unsigned) (l_year % 100));
		s_fao$put_chr (a_ctx, ' ');
		}

	s_fao$put_2d  (a_ctx, l_hh);
	s_fao$put_chr (a_ctx, ':');
	s_fao$put_2d  (a_ctx, l_mm);
	s_fao$put_chr (a_ctx, ':');
	s_fao$put_2d  (a_ctx, l_ss);
	s_fao$put_raw (a_ctx, ".00", 3);
}

/*
**++
**  S_FAO$GET_NUMPRM - fetch a numeric operand of the given size.
**
**	B/W/L operands are passed by value in the parameter list and
**	are masked/sign-extended to the requested size.  A quadword (Q)
**	operand, exactly as on OpenVMS, is passed BY REFERENCE: the
**	parameter is the address of an int64_t/uint64_t.
**
**  INPUT:
**	a_prm	- address of the shared parameter state
**	a_size	- operand size code: 'B', 'W', 'L', 'Q'
**	a_signed- non-zero: sign-extend the value
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	64-bit value of the operand
**--
*/
static	uint64_t s_fao$get_numprm (fao_prmstate_t *a_prm, char a_size,
				int a_signed)
{
fao_prm_t l_prm = s_fao$get_prm (a_prm);

	switch ( a_size )
		{
		case 'B':
			return	a_signed ? (uint64_t)(int64_t)(int8_t)  l_prm
					: (uint64_t)(uint8_t)  l_prm;
		case 'W':
			return	a_signed ? (uint64_t)(int64_t)(int16_t) l_prm
					: (uint64_t)(uint16_t) l_prm;
		case 'L':
			return	a_signed ? (uint64_t)(int64_t)(int32_t) l_prm
					: (uint64_t)(uint32_t) l_prm;
		case 'Q':
			if ( ! l_prm )
				return	0;
			return	* (const uint64_t *) l_prm;
		}

	return	0;
}

/*
**++
**  S_FAO$FIND_GROUP_END - find the terminator of a !n(...) or !n<...!>
**  group, honouring nesting of the same kind.
**
**  INPUT:
**	a_ctl	- address of the first byte AFTER the group opener
**	a_open	- opening character: '(' or '<'
**	a_close	- closing character: ')' or '>'
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	Address of the '!' of the matching "!)"/"!>" terminator, or NULL
**	if the terminator has not been found.
**--
*/
static	const char *s_fao$find_group_end (const char *a_ctl,
				char a_open, char a_close)
{
int	l_depth = 1;
const char *l_ptr = a_ctl;

	while ( *l_ptr )
		{
		if ( (l_ptr [0] == '!') && l_ptr [1] )
			{
			const char *l_dir = l_ptr + 1;

			while ( $FAO_ISDIGIT (*l_dir) || (*l_dir == '#') )
				l_dir++;

			if ( *l_dir == a_open )
				l_depth++;
			else if ( *l_dir == a_close )
				{
				if ( --l_depth == 0 )
					return	l_ptr;
				}

			l_ptr = l_dir + 1;
			continue;
			}

		l_ptr++;
		}

	return	NULL;
}

/*
**++
**  S_FAO$PROCESS - the FAO engine: scan the control string fragment and
**  emit the formatted output into the context buffer.
**
**	Literal runs between directives are located first and emitted
**	in bulk with a single bounds check.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_ctl	- address of the control string fragment
**	a_end	- address past the last byte of the fragment,
**		  NULL - process up to the terminating zero byte
**	a_depth	- current group nesting depth (recursion guard)
**
**  OUTPUT:
**	NONE
**
**  RETURN:
**	Address past the processed fragment; on a malformed directive
**	the sticky status of the context is set to FAO$K_BADDIR and
**	processing continues from the next byte.
**--
*/
static	const char *s_fao$process (fao_ctx_t *a_ctx, const char *a_ctl,
				const char *a_end, int a_depth)
{
const char *l_ptr = a_ctl, *l_run, *l_grpend, *l_str;
fao_prmstate_t *l_prm = a_ctx->prm;
const fao_desc_t *l_dsc;
fao_prm_t l_arg;
fao_ctx_t l_sub;
uint64_t l_val;
size_t	l_len, l_sublen;
int	l_width, l_rpt, l_signed, l_base, l_defdig, l_szidx;
char	l_dir, l_size;
char	l_fldbuf [FAO$K_FLDBUF];

#define	$FAO_MORE()	( (a_end == NULL) ? (*l_ptr != 0) : (l_ptr < a_end) )

	while ( $FAO_MORE () )
		{
		/* Bulk copy of the literal run up to the next '!' */
		if ( *l_ptr != '!' )
			{
			l_run = l_ptr;

			do	l_ptr++;
			while ( $FAO_MORE () && (*l_ptr != '!') );

			s_fao$put_raw (a_ctx, l_run, (size_t)(l_ptr - l_run));
			continue;
			}

		l_ptr++;			/* Skip the '!'		*/
		l_width = FAO$K_WIDTHAUTO;

		/* Optional width/count: decimal digits or '#' */
		if ( *l_ptr == '#' )
			{
			l_width = (int) (uint32_t) s_fao$get_prm (l_prm);
			l_ptr++;
			}
		else if ( $FAO_ISDIGIT (*l_ptr) )
			{
			l_width = 0;

			do	l_width = l_width * 10 + (*l_ptr++ - '0');
			while ( $FAO_ISDIGIT (*l_ptr) );
			}

		l_dir = *l_ptr++;

		switch ( l_dir )
			{
			case '!':		/* Literal '!'		*/
				s_fao$put_chr (a_ctx, '!');
				break;

			case '/':		/* CR+LF		*/
				s_fao$put_raw (a_ctx, "\r\n", 2);
				break;

			case '_':		/* Horizontal tab	*/
				s_fao$put_chr (a_ctx, '\t');
				break;

			case '^':		/* Form feed		*/
				s_fao$put_chr (a_ctx, '\f');
				break;

			case '-':		/* Reuse last n params	*/
				l_rpt = (l_width == FAO$K_WIDTHAUTO)
					? 1 : l_width;
				l_prm->prmidx -= l_rpt;

				if ( l_prm->prmidx < 0 )
					l_prm->prmidx = 0;
				break;

			case '+':		/* Skip next n params	*/
				l_rpt = (l_width == FAO$K_WIDTHAUTO)
					? 1 : l_width;

				while ( l_rpt-- > 0 )
					(void) s_fao$get_prm (l_prm);
				break;

			case '%':		/* !%S !%T !%D		*/
				switch ( *l_ptr++ )
					{
					case 'S':
						if ( l_prm->lastnum != 1 )
							s_fao$put_chr (a_ctx, 's');
						break;

					case 'T':
						s_fao$put_time (a_ctx,
							s_fao$get_prm (l_prm), 0);
						break;

					case 'D':
						s_fao$put_time (a_ctx,
							s_fao$get_prm (l_prm), 1);
						break;

					default:
						a_ctx->sts = FAO$K_BADDIR;
						$LOG (STS$K_ERROR,
							"Bad !%% directive at offset %ld",
							(long) (l_ptr - a_ctl));
						break;
					}
				break;

			case '*':		/* !n*c - repeat char	*/
				s_fao$put_fill (a_ctx, *l_ptr++,
					(l_width == FAO$K_WIDTHAUTO)
						? 1 : (size_t) l_width);
				break;

			case '(':		/* !n(...) - repeat group */
				{
				l_grpend = s_fao$find_group_end (l_ptr, '(', ')');

				if ( (l_grpend == NULL)
					|| (a_depth >= FAO$K_MAXDEPTH) )
					{
					a_ctx->sts = FAO$K_BADDIR;
					$LOG (STS$K_ERROR,
						"Unterminated or too deep !n( ) group");
					break;
					}

				l_rpt = (l_width == FAO$K_WIDTHAUTO)
					? 1 : l_width;

				while ( l_rpt-- > 0 )
					(void) s_fao$process (a_ctx, l_ptr,
						l_grpend, a_depth + 1);

				l_ptr = l_grpend + 2;	/* Past "!)"	*/
				break;
				}

			case '<':		/* !n<...!> - fixed field */
				{
				l_grpend = s_fao$find_group_end (l_ptr, '<', '>');

				if ( (l_grpend == NULL)
					|| (a_depth >= FAO$K_MAXDEPTH) )
					{
					a_ctx->sts = FAO$K_BADDIR;
					$LOG (STS$K_ERROR,
						"Unterminated or too deep !n< > group");
					break;
					}

				/*
				** Render the sub-fragment into a scratch
				** buffer; the parameter state is shared
				** by reference, nothing is copied back.
				*/
				l_sub.obase	= l_fldbuf;
				l_sub.optr	= l_fldbuf;
				l_sub.oend	= l_fldbuf + sizeof(l_fldbuf);
				l_sub.prm	= l_prm;
				l_sub.sts	= STS$K_SUCCESS;

				(void) s_fao$process (&l_sub, l_ptr,
						l_grpend, a_depth + 1);

				if ( l_sub.sts == FAO$K_BADDIR )
					a_ctx->sts = FAO$K_BADDIR;

				l_sublen = (size_t)(l_sub.optr - l_fldbuf);

				s_fao$put_str (a_ctx, l_fldbuf, l_sublen,
					(l_width == FAO$K_WIDTHAUTO)
						? (int) l_sublen : l_width,
					0);

				l_ptr = l_grpend + 2;	/* Past "!>"	*/
				break;
				}

			case 'A':		/* String directives	*/
				{
				switch ( *l_ptr++ )
					{
					case 'S':	/* By descriptor */
						l_arg = s_fao$get_prm (l_prm);
						l_dsc = (const fao_desc_t *) l_arg;

						if ( l_dsc )
							s_fao$put_str (a_ctx,
								l_dsc->dsc$a_pointer,
								l_dsc->dsc$w_length,
								l_width, 0);
						break;

					case 'Z':	/* Zero-terminated */
						l_arg = s_fao$get_prm (l_prm);
						l_str = (const char *) l_arg;

						if ( l_str )
							s_fao$put_str (a_ctx, l_str,
								s_fao$strlen (l_str),
								l_width, 0);
						break;

					case 'C':	/* Counted string */
						l_arg = s_fao$get_prm (l_prm);
						l_str = (const char *) l_arg;

						if ( l_str )
							s_fao$put_str (a_ctx,
								l_str + 1,
								(unsigned char) l_str [0],
								l_width, 0);
						break;

					case 'D':	/* Length, address */
					case 'F':
						l_len = (size_t)(uint32_t)
							s_fao$get_prm (l_prm);
						l_str = (const char *)
							s_fao$get_prm (l_prm);

						if ( l_str )
							s_fao$put_str (a_ctx, l_str,
								l_len, l_width,
								(l_ptr [-1] == 'F'));
						break;

					default:
						a_ctx->sts = FAO$K_BADDIR;
						$LOG (STS$K_ERROR,
							"Bad !A directive at offset %ld",
							(long) (l_ptr - a_ctl));
						break;
					}
				break;
				}

			case 'U':		/* Unsigned decimal	*/
			case 'S':		/* Signed decimal	*/
			case 'Z':		/* Zero-filled decimal	*/
			case 'X':		/* Hexadecimal		*/
			case 'O':		/* Octal		*/
				{
				l_size	= *l_ptr++;
				l_signed = (l_dir == 'S');
				l_base	= 10;
				l_defdig = 0;

				switch ( l_size )
					{
					case 'B':	l_szidx = 0;	break;
					case 'W':	l_szidx = 1;	break;
					case 'L':	l_szidx = 2;	break;
					case 'Q':	l_szidx = 3;	break;
					default:
						a_ctx->sts = FAO$K_BADDIR;
						$LOG (STS$K_ERROR,
							"Bad size in !%c directive",
							l_dir);
						continue;
					}

				l_val = s_fao$get_numprm (l_prm, l_size, l_signed);

				if ( l_dir == 'X' )
					{
					l_base = 16;
					l_defdig = s_hexdef [l_szidx];
					}
				else if ( l_dir == 'O' )
					{
					l_base = 8;
					l_defdig = s_octdef [l_szidx];
					}
				else if ( l_dir == 'Z' )
					l_defdig = (l_width == FAO$K_WIDTHAUTO)
						? 0 : l_width;

				s_fao$put_num (a_ctx, l_val, l_base, l_signed,
					l_width, l_defdig);
				break;
				}

			default:
				a_ctx->sts = FAO$K_BADDIR;
				$LOG (STS$K_ERROR,
					"Unknown FAO directive '!%c'", l_dir);
				break;
			}
		}

#undef	$FAO_MORE

	return	l_ptr;
}

/*
**++
**  UTIL$FAO - portable variant of the OpenVMS Formatted ASCII Output
**  system service, varargs form.
**
**	NOTE: every variadic parameter MUST be explicitly cast by the
**	caller to fao_prm_t (integers) or be a pointer; a quadword
**	(!..Q) parameter is the ADDRESS of a 64-bit integer.
**
**  INPUT:
**	a_ctlstr - address of the zero-terminated control string with
**		   FAO directives (see the module abstract)
**	a_outlen - optional address of a word to receive the length of
**		   the resulting string, may be NULL
**	a_outbuf - address of the output buffer descriptor (fao_desc_t):
**		   dsc$w_length is the buffer capacity on input
**	...	 - conversion parameters, each of type fao_prm_t
**
**  OUTPUT:
**	Buffer described by a_outbuf - the resulting string (not
**	zero-terminated, as on OpenVMS); *a_outlen - its length
**
**  RETURN:
**	Condition value:
**	  STS$K_SUCCESS - conversion complete
**	  FAO$K_BUFOVF  - output truncated to the buffer capacity
**	  FAO$K_BADDIR  - malformed directive has been encountered
**	  STS$K_ERROR   - bad routine arguments
**--
*/
int	__util$fao		(const char *a_ctlstr, unsigned short *a_outlen,
			fao_desc_t *a_outbuf, ...)
{
fao_prmstate_t l_prm;
fao_ctx_t l_ctx;
va_list	l_ap;

	if ( (a_ctlstr == NULL) || (a_outbuf == NULL)
		|| (a_outbuf->dsc$a_pointer == NULL) )
		return	STS$K_ERROR;

	va_start (l_ap, a_outbuf);

	l_prm.prmlst	= NULL;
	l_prm.prmlstcnt	= 0;
	l_prm.ap	= &l_ap;
	l_prm.prmfetched = 0;
	l_prm.prmidx	= 0;
	l_prm.lastnum	= 0;

	l_ctx.obase	= a_outbuf->dsc$a_pointer;
	l_ctx.optr	= l_ctx.obase;
	l_ctx.oend	= l_ctx.obase + a_outbuf->dsc$w_length;
	l_ctx.prm	= &l_prm;
	l_ctx.sts	= STS$K_SUCCESS;

	(void) s_fao$process (&l_ctx, a_ctlstr, NULL, 0);

	va_end (l_ap);

	if ( a_outlen != NULL )
		*a_outlen = (unsigned short) (l_ctx.optr - l_ctx.obase);

	return	l_ctx.sts;
}

/*
**++
**  UTIL$FAOL - portable variant of the OpenVMS Formatted ASCII Output
**  system service, list form.
**
**  INPUT:
**	a_ctlstr - address of the zero-terminated control string
**	a_outlen - optional address of a word to receive the length of
**		   the resulting string, may be NULL
**	a_outbuf - address of the output buffer descriptor (fao_desc_t)
**	a_prmlst - address of the parameter array (fao_prm_t elements)
**	a_prmcnt - number of elements in the array; unlike OpenVMS the
**		   count is explicit, out of range access yields 0
**
**  OUTPUT:
**	Buffer described by a_outbuf - the resulting string;
**	*a_outlen - its length
**
**  RETURN:
**	See UTIL$FAO
**--
*/
int	__util$faol	(const char *a_ctlstr, unsigned short *a_outlen,
			fao_desc_t *a_outbuf,
			const fao_prm_t *a_prmlst, int a_prmcnt)
{
fao_prmstate_t l_prm;
fao_ctx_t l_ctx;
static	const fao_prm_t s_empty [1] = { 0 };

	if ( (a_ctlstr == NULL) || (a_outbuf == NULL)
		|| (a_outbuf->dsc$a_pointer == NULL) )
		return	STS$K_ERROR;

	l_prm.prmlst	= (a_prmlst != NULL) ? a_prmlst : s_empty;
	l_prm.prmlstcnt	= (a_prmlst != NULL) ? a_prmcnt : 0;
	l_prm.ap	= NULL;
	l_prm.prmfetched = 0;
	l_prm.prmidx	= 0;
	l_prm.lastnum	= 0;

	l_ctx.obase	= a_outbuf->dsc$a_pointer;
	l_ctx.optr	= l_ctx.obase;
	l_ctx.oend	= l_ctx.obase + a_outbuf->dsc$w_length;
	l_ctx.prm	= &l_prm;
	l_ctx.sts	= STS$K_SUCCESS;

	(void) s_fao$process (&l_ctx, a_ctlstr, NULL, 0);

	if ( a_outlen != NULL )
		*a_outlen = (unsigned short) (l_ctx.optr - l_ctx.obase);

	return	l_ctx.sts;
}

/*
**++
**  S_FAO$TIM_NUM - parse an unsigned decimal number of the time string.
**
**  INPUT:
**	a_ptr	- address of the scan pointer, advanced past the digits
**
**  OUTPUT:
**	*a_val	- parsed value; unchanged when no digits are present
**
**  RETURN:
**	Number of digits consumed (0 - field omitted)
**--
*/
static	int	s_fao$tim_num	(const char **a_ptr, unsigned *a_val)
{
const char *l_ptr = *a_ptr;
unsigned l_val = 0;
int	l_cnt = 0;

	while ( $FAO_ISDIGIT (*l_ptr) && (l_cnt < 9) )
		{
		l_val = l_val * 10 + (unsigned) (*l_ptr++ - '0');
		l_cnt++;
		}

	if ( l_cnt )
		*a_val = l_val;

	*a_ptr = l_ptr;
	return	l_cnt;
}

/*
**++
**  S_FAO$TIM_HMS - parse the time-of-day part "hh:mm[:ss[.cc]]" with
**  optionally omitted fields ("::10", "12:", ...), OpenVMS style.
**
**  INPUT:
**	a_ptr	- address of the scan pointer, advanced past the field
**
**  OUTPUT:
**	*a_secs	- seconds since midnight
**
**  RETURN:
**	STS$K_SUCCESS or FAO$K_IVTIME on a range violation
**--
*/
static	int	s_fao$tim_hms	(const char **a_ptr, unsigned *a_secs)
{
const char *l_ptr = *a_ptr;
unsigned l_hh = 0, l_mm = 0, l_ss = 0, l_cc = 0;

	(void) s_fao$tim_num (&l_ptr, &l_hh);

	if ( *l_ptr == ':' )
		{
		l_ptr++;
		(void) s_fao$tim_num (&l_ptr, &l_mm);

		if ( *l_ptr == ':' )
			{
			l_ptr++;
			(void) s_fao$tim_num (&l_ptr, &l_ss);

			if ( *l_ptr == '.' )
				{
				l_ptr++;
				(void) s_fao$tim_num (&l_ptr, &l_cc);
				}
			}
		}

	if ( (l_hh > 23) || (l_mm > 59) || (l_ss > 59) || (l_cc > 99) )
		return	FAO$K_IVTIME;

	*a_secs	= l_hh * 3600 + l_mm * 60 + l_ss;	/* Hundredths dropped */
	*a_ptr	= l_ptr;
	return	STS$K_SUCCESS;
}

/*
**++
**  S_FAO$TIM_MONTH - match a case-insensitive three-letter month name.
**
**  INPUT:
**	a_ptr	- address of the scan pointer, advanced past the name
**
**  OUTPUT:
**	*a_mon	- month number, 1..12
**
**  RETURN:
**	STS$K_SUCCESS or FAO$K_IVTIME when the name is not recognized
**--
*/
static	int	s_fao$tim_month	(const char **a_ptr, unsigned *a_mon)
{
const char *l_ptr = *a_ptr;
char	l_name [3];
unsigned l_i;

	for ( l_i = 0; l_i < 3; l_i++ )
		{
		char	l_chr = l_ptr [l_i];

		if ( (l_chr >= 'a') && (l_chr <= 'z') )
			l_chr = (char) (l_chr - 0x20);

		if ( (l_chr < 'A') || (l_chr > 'Z') )
			return	FAO$K_IVTIME;

		l_name [l_i] = l_chr;
		}

	for ( l_i = 0; l_i < 12; l_i++ )
		if ( (l_name [0] == s_months [l_i][0])
			&& (l_name [1] == s_months [l_i][1])
			&& (l_name [2] == s_months [l_i][2]) )
			{
			*a_mon	= l_i + 1;
			*a_ptr	= l_ptr + 3;
			return	STS$K_SUCCESS;
			}

	return	FAO$K_IVTIME;
}

/*
**++
**  UTIL$BINTIM - portable variant of the OpenVMS SYS$BINTIM system
**  service: convert an ASCII time string to binary time.
**
**	Unlike OpenVMS (quadword of 100ns ticks since 17-NOV-1858) the
**	binary time here is fao_time_t: seconds of the UNIX epoch, i.e.
**	exactly what the !%T/!%D directives of UTIL$FAO consume, so the
**	pair is round-trip consistent.  Hundredths of a second are
**	parsed and validated but dropped.
**
**	Accepted grammar (leading/trailing blanks and tabs ignored):
**
**	  Absolute: "dd-MMM-yyyy[ hh:mm[:ss[.cc]]]"
**		Result: seconds of the epoch; the string is interpreted
**		in the FAO$K_TZOFFSET timezone (mirror of !%D output).
**		Omitted date fields ("-MMM-", "dd-MMM-") default to the
**		current date values, omitted time defaults to 00:00:00.
**
**	  Absolute, time only: "hh:mm[:ss[.cc]]"
**		The current date is assumed.
**
**	  Delta:    "dddd hh:mm[:ss[.cc]]"  or  "dddd-hh:mm[:ss[.cc]]"
**		Result is NEGATIVE (OpenVMS convention for delta times):
**		-(dddd*86400 + hh*3600 + mm*60 + ss).
**
**	Time-of-day fields can be individually omitted ("::10" is ten
**	seconds), matching the OpenVMS rules.
**
**  INPUT:
**	a_timstr - address of the zero-terminated time string
**	a_timadr - address of the fao_time_t to receive the result
**
**  OUTPUT:
**	*a_timadr - binary time (see above); unchanged on error
**
**  RETURN:
**	Condition value:
**	  STS$K_SUCCESS - conversion complete
**	  FAO$K_IVTIME  - syntax error or field out of range
**	  STS$K_ERROR   - bad routine arguments
**--
*/
int	__util$bintim	(const char *a_timstr, fao_time_t *a_timadr)
{
const char *l_ptr = a_timstr;
unsigned l_num = 0, l_mon, l_secs = 0;
unsigned l_day, l_cmon, l_cday, l_max;
int64_t	l_year, l_cyear, l_now;
int	l_numdig, l_sts;

	if ( (a_timstr == NULL) || (a_timadr == NULL) )
		return	STS$K_ERROR;

	while ( (*l_ptr == ' ') || (*l_ptr == '\t') )
		l_ptr++;

	l_numdig = s_fao$tim_num (&l_ptr, &l_num);

	/*
	** Delta time: "dddd hh:mm:ss" / "dddd-hh:mm:ss" - a day count
	** followed by a blank or a dash and a time-of-day with a colon.
	*/
	if ( l_numdig && ((*l_ptr == ' ') || (*l_ptr == '\t')
		|| ((*l_ptr == '-') && ($FAO_ISDIGIT (l_ptr [1])
					|| (l_ptr [1] == ':')))) )
		{
		l_ptr++;

		while ( (*l_ptr == ' ') || (*l_ptr == '\t') )
			l_ptr++;

		if ( (l_sts = s_fao$tim_hms (&l_ptr, &l_secs)) != STS$K_SUCCESS )
			return	l_sts;

		while ( (*l_ptr == ' ') || (*l_ptr == '\t') )
			l_ptr++;

		if ( *l_ptr )
			return	FAO$K_IVTIME;

		*a_timadr = (fao_time_t)
			( - ((int64_t) l_num * 86400 + (int64_t) l_secs) );
		return	STS$K_SUCCESS;
		}

	/* Current date: default source for the omitted absolute fields */
	l_now = (int64_t) FAO$GETTIME () + FAO$K_TZOFFSET;
	s_fao$days_to_civil (l_now / 86400 - (l_now % 86400 < 0),
		&l_cyear, &l_cmon, &l_cday);

	l_day	= l_cday;
	l_mon	= l_cmon;
	l_year	= l_cyear;

	if ( *l_ptr == '-' )
		{
		/* Absolute date: [dd]-[MMM]-[yyyy] */
		if ( l_numdig )
			l_day = l_num;

		l_ptr++;

		if ( (l_sts = s_fao$tim_month (&l_ptr, &l_mon)) != STS$K_SUCCESS )
			return	l_sts;

		if ( *l_ptr++ != '-' )
			return	FAO$K_IVTIME;

		if ( s_fao$tim_num (&l_ptr, &l_num) )
			l_year = (int64_t) l_num;

		while ( (*l_ptr == ' ') || (*l_ptr == '\t') )
			l_ptr++;

		if ( *l_ptr )
			if ( (l_sts = s_fao$tim_hms (&l_ptr, &l_secs))
				!= STS$K_SUCCESS )
				return	l_sts;
		}
	else if ( l_numdig || (*l_ptr == ':') )
		{
		/* Absolute, time only: today at hh:mm[:ss[.cc]] */
		l_ptr = a_timstr;

		while ( (*l_ptr == ' ') || (*l_ptr == '\t') )
			l_ptr++;

		if ( (l_sts = s_fao$tim_hms (&l_ptr, &l_secs)) != STS$K_SUCCESS )
			return	l_sts;
		}
	else	return	FAO$K_IVTIME;

	while ( (*l_ptr == ' ') || (*l_ptr == '\t') )
		l_ptr++;

	if ( *l_ptr )
		return	FAO$K_IVTIME;

	/* Validate the day against the month length (leap-aware) */
	l_max = s_mdays [l_mon - 1];

	if ( (l_mon == 2) && ( ((l_year % 4 == 0) && (l_year % 100 != 0))
				|| (l_year % 400 == 0) ) )
		l_max = 29;

	if ( (l_day < 1) || (l_day > l_max) )
		return	FAO$K_IVTIME;

	*a_timadr = (fao_time_t)
		( s_fao$days_from_civil (l_year, l_mon, l_day) * 86400
		+ (int64_t) l_secs - FAO$K_TZOFFSET );

	return	STS$K_SUCCESS;
}

/*
**++
**  UTIL$GETTIM - portable variant of the OpenVMS SYS$GETTIM system
**  service: return the current binary time.
**
**  INPUT:
**	a_timadr - address of the fao_time_t to receive the result
**
**  OUTPUT:
**	*a_timadr - current time, seconds of the UNIX epoch (UTC)
**
**  RETURN:
**	Condition value:
**	  STS$K_SUCCESS - always, unless a_timadr is NULL
**	  STS$K_ERROR   - bad routine arguments
**--
*/
int	__util$gettim	(fao_time_t *a_timadr)
{
	if ( a_timadr == NULL )
		return	STS$K_ERROR;

	*a_timadr = (fao_time_t) FAO$GETTIME ();

	return	STS$K_SUCCESS;
}

/*
**++
**  UTIL$ASCTIM - portable variant of the OpenVMS SYS$ASCTIM system
**  service: convert binary time to an ASCII string.
**
**	Output formats (exact mirror of the !%D/!%T directives, so
**	UTIL$BINTIM accepts the result back - round-trip consistent):
**
**	  Absolute (value >= 0), a_cvtflg = 0:
**		"dd-MMM-yyyy hh:mm:ss.00"  (day blank-padded to 2)
**	  Absolute, a_cvtflg = 1:
**		"hh:mm:ss.00"
**	  Delta (value < 0, OpenVMS convention), a_cvtflg = 0:
**		"dddd hh:mm:ss.00"
**	  Delta, a_cvtflg = 1:
**		"hh:mm:ss.00"  (the day count is discarded)
**
**	The absolute value is rendered in the FAO$K_TZOFFSET timezone.
**
**  INPUT:
**	a_outlen - optional address of a word to receive the length of
**		   the resulting string, may be NULL
**	a_outbuf - address of the output buffer descriptor (fao_desc_t)
**	a_timadr - address of the binary time; NULL - the current time
**		   is used, as on OpenVMS
**	a_cvtflg - conversion flag: 0 - full, 1 - time-of-day only
**
**  OUTPUT:
**	Buffer described by a_outbuf - the resulting string (not
**	zero-terminated); *a_outlen - its length
**
**  RETURN:
**	Condition value:
**	  STS$K_SUCCESS - conversion complete
**	  FAO$K_BUFOVF  - output truncated to the buffer capacity
**	  STS$K_ERROR   - bad routine arguments
**--
*/
int	__util$asctim	(unsigned short *a_outlen, fao_desc_t *a_outbuf,
			const fao_time_t *a_timadr, int a_cvtflg)
{
fao_ctx_t l_ctx;
fao_prmstate_t l_prm = {0};
fao_time_t l_tim;
int64_t	l_val, l_mag, l_secs;
char	l_buf [FAO$K_NUMBUF];
char	*l_dig;

	if ( (a_outbuf == NULL) || (a_outbuf->dsc$a_pointer == NULL) )
		return	STS$K_ERROR;

	if ( a_timadr != NULL )
		l_tim = *a_timadr;
	else	l_tim = (fao_time_t) FAO$GETTIME ();

	l_val = (int64_t) l_tim;

	l_ctx.obase	= a_outbuf->dsc$a_pointer;
	l_ctx.optr	= l_ctx.obase;
	l_ctx.oend	= l_ctx.obase + a_outbuf->dsc$w_length;
	l_ctx.prm	= &l_prm;
	l_ctx.sts	= STS$K_SUCCESS;

	if ( l_val >= 0 )			/* Absolute time	*/
		s_fao$put_time (&l_ctx, (fao_prm_t) &l_tim, ! a_cvtflg);
	else	{				/* Delta time		*/
		l_mag	= - l_val;
		l_secs	= l_mag % 86400;

		if ( ! a_cvtflg )
			{
			l_dig = s_fao$cvt_dec ((uint64_t)(l_mag / 86400),
					l_buf + FAO$K_NUMBUF);

			s_fao$put_raw (&l_ctx, l_dig,
				(size_t)(l_buf + FAO$K_NUMBUF - l_dig));
			s_fao$put_chr (&l_ctx, ' ');
			}

		s_fao$put_2d  (&l_ctx, (unsigned) (l_secs / 3600));
		s_fao$put_chr (&l_ctx, ':');
		s_fao$put_2d  (&l_ctx, (unsigned) ((l_secs / 60) % 60));
		s_fao$put_chr (&l_ctx, ':');
		s_fao$put_2d  (&l_ctx, (unsigned) (l_secs % 60));
		s_fao$put_raw (&l_ctx, ".00", 3);
		}

	if ( a_outlen != NULL )
		*a_outlen = (unsigned short) (l_ctx.optr - l_ctx.obase);

	return	l_ctx.sts;
}

/*
**++
**  UTIL$ADD_TIMES - portable variant of the OpenVMS LIB$ADD_TIMES
**  routine: add two binary times.
**
**	OpenVMS combination rules (delta times are negative):
**	  delta + delta    -> delta
**	  absolute + delta -> absolute (delta magnitude is added)
**	  absolute + absolute -> error, at least one delta is required
**
**  INPUT:
**	a_tim1	- address of the first operand
**	a_tim2	- address of the second operand
**
**  OUTPUT:
**	*a_result - the sum; unchanged on error
**
**  RETURN:
**	Condition value:
**	  STS$K_SUCCESS - operation complete
**	  FAO$K_IVTIME  - both operands are absolute times
**	  STS$K_ERROR   - bad routine arguments
**--
*/
int	__util$add_times	(const fao_time_t *a_tim1, const fao_time_t *a_tim2,
			fao_time_t *a_result)
{
int64_t	l_t1, l_t2;

	if ( (a_tim1 == NULL) || (a_tim2 == NULL) || (a_result == NULL) )
		return	STS$K_ERROR;

	l_t1 = (int64_t) *a_tim1;
	l_t2 = (int64_t) *a_tim2;

	if ( (l_t1 >= 0) && (l_t2 >= 0) )	/* Two absolute times	*/
		return	FAO$K_IVTIME;

	if ( (l_t1 < 0) && (l_t2 < 0) )		/* Two delta times	*/
		{
		*a_result = (fao_time_t) (l_t1 + l_t2);
		return	STS$K_SUCCESS;
		}

	/* Absolute + delta: the delta magnitude advances the time */
	if ( l_t1 >= 0 )
		*a_result = (fao_time_t) (l_t1 - l_t2);
	else	*a_result = (fao_time_t) (l_t2 - l_t1);

	return	STS$K_SUCCESS;
}

/*
**++
**  UTIL$SUB_TIMES - portable variant of the OpenVMS LIB$SUB_TIMES
**  routine: subtract the second binary time from the first.
**
**	OpenVMS combination rules (delta times are negative):
**	  absolute - absolute -> delta; tim1 must not precede tim2
**	  absolute - delta    -> absolute (delta magnitude subtracted);
**		the result must not become negative
**	  delta - delta       -> delta; |tim1| must not be less than
**		|tim2|
**	  delta - absolute    -> error
**
**  INPUT:
**	a_tim1	- address of the minuend
**	a_tim2	- address of the subtrahend
**
**  OUTPUT:
**	*a_result - the difference; unchanged on error
**
**  RETURN:
**	Condition value:
**	  STS$K_SUCCESS - operation complete
**	  FAO$K_NEGTIM  - the result would be negative (LIB$_NEGTIM)
**	  FAO$K_IVTIME  - delta minus absolute is requested
**	  STS$K_ERROR   - bad routine arguments
**--
*/
int	__util$sub_times	(const fao_time_t *a_tim1, const fao_time_t *a_tim2,
			fao_time_t *a_result)
{
int64_t	l_t1, l_t2;

	if ( (a_tim1 == NULL) || (a_tim2 == NULL) || (a_result == NULL) )
		return	STS$K_ERROR;

	l_t1 = (int64_t) *a_tim1;
	l_t2 = (int64_t) *a_tim2;

	if ( l_t1 >= 0 )
		{
		if ( l_t2 >= 0 )		/* Absolute - absolute	*/
			{
			if ( l_t1 < l_t2 )
				return	FAO$K_NEGTIM;

			*a_result = (fao_time_t) ( - (l_t1 - l_t2) );
			return	STS$K_SUCCESS;
			}

		/* Absolute - delta: the delta magnitude rolls time back */
		if ( l_t1 + l_t2 < 0 )
			return	FAO$K_NEGTIM;

		*a_result = (fao_time_t) (l_t1 + l_t2);
		return	STS$K_SUCCESS;
		}

	if ( l_t2 >= 0 )			/* Delta - absolute	*/
		return	FAO$K_IVTIME;

	if ( l_t1 > l_t2 )			/* |tim1| < |tim2|	*/
		return	FAO$K_NEGTIM;

	*a_result = (fao_time_t) (l_t1 - l_t2);
	return	STS$K_SUCCESS;
}
