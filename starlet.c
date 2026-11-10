/*
**++
**
**  FACILITY:	STARLET - Portable analogues of OpenVMS system services
**
**  MODULE:	starlet.c
**
**  ABSTRACT:	Portable (ISO C, freestanding-friendly) UTIL$FAO/UTIL$FAOL (SYS$FAO/SYS$FAOL), UTIL$BINTIM,
**		UTIL$GETTIM, UTIL$ASCTIM (SYS$BINTIM/GETTIM/ASCTIM), UTIL$ADD_TIMES and UTIL$SUB_TIMES
**		(LIB$ADD_TIMES/LIB$SUB_TIMES).  No CRTL beyond time(), which FAO$GETTIME may replace.  Binary
**		time is fao_time_t, seconds of the UNIX epoch: a negative value is a delta time, zero means
**		"now".  $LOG is a no-op here: the formatter must not log through itself, errors go to the caller
**		as the returned status.  Directives and parameters: see starlet.h and the doc/ manuals.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  24-JUL-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-16		31-OCT-2026	CLD
**		1.7.0 defect pass: bounded directive scanner (truncated directive = FAO$K_BADDIR), negative time = delta in !%D/!%T,
**		zero = "now", year > 9999 printed in full, new FAO$K_FLDOVF/FAO$K_NOPRM, first error wins, FAO$K_* real condition values.
**
**	X.01-15		13-AUG-2026	RRL
**		!%6 shows an IPv4-mapped address as ::ffff:a.b.c.d (RFC 5952 par.5, tests/test_fao_addr.c); __REV__ brought in step with __IDENT__.
**
**	X.01-15ECO1	14-AUG-2026	RRL
**		The address-directive helpers brought to the house routine-body template; layout only.
**
**	X.01-14		13-AUG-2026	RRL
**		!%6 emits the RFC 5952 canonical form: the longest run of two or more zero groups compressed to "::".
**
**	X.01-13		07-AUG-2026	RRL
**		Added the address directives !%M (MAC), !%I (IPv4), !%6 (IPv6) and !%U (UUID); a NULL pointer yields "(null)".
**
**	X.01-12		01-AUG-2026	RRL
**		Time-string helpers are checked by the odd/even convention (!(1 & status)) rather than against STS$K_SUCCESS.
**
**	X.01-11		01-AUG-2026	RRL
**		__util$asctim() passed the address of its time to s_fao$put_time(), which takes it BY VALUE; pass the value.
**
**	X.01-10		01-AUG-2026	RRL
**		FAO$K_BUFOVF 5 -> 0xA and FAO$K_IVTIME 7 -> 0xC: the odd values read as success under '1 & status'.
**
**	X.01-09		30-JUL-2026	RRL
**		64-bit parameters BY VALUE: fao_prm_t is uint64_t, !xQ and !%D/!%T read the value, not a pointer.
**
**	X.01-08		27-JUL-2026	RRL
**		FAO routines renamed util$fao/util$faol -> __util$fao/__util$faol.
**
**	X.01-07		27-JUL-2026	RRL
**		Time routines renamed util$ -> __util$.
**
**	X.01-06		27-JUL-2026	CLAUDE
**		avproto declaration style (locals at the top, tables s_hexdef/s_octdef/s_mdays at module scope); no functional change.
**
**	X.01-05		27-JUL-2026	CLAUDE
**		Static routines renamed with the s_ prefix (fao$$name -> s_fao$name); no functional change.
**
**	X.01-04		24-JUL-2026	CLAUDE
**		UTIL$GETTIM, UTIL$ASCTIM, UTIL$ADD_TIMES, UTIL$SUB_TIMES added; FAO$K_NEGTIM status added.
**
**	X.01-03		24-JUL-2026	CLAUDE
**		Module renamed fao.c -> starlet.c (facility STARLET); no functional change.
**
**	X.01-02		24-JUL-2026	CLAUDE
**		Routine prefix sys$ -> util$; UTIL$BINTIM added; calendar conversions factored out (S_FAO$DAYS_TO/FROM_CIVIL).
**
**	X.01-01		24-JUL-2026	CLAUDE
**		CRTL dependencies removed (stdio/string/ctype/localtime); private date conversion, bulk literal copy, two-digit decimal conversion.
**
**	X.01-00		24-JUL-2026	CLAUDE
**		Initial version.
**
**--
*/

#define	__MODULE__	"STARLET"
#define	__IDENT__	"X.01-16"
#define	__REV__		"1.16.0"

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
**  starlet.h now brings utility_routines.h in (the STS$K_* values), and with
**  it the real $LOG.  The formatter must stay silent - a log line from inside
**  the engine that $LOG_FAO itself runs on would be noise at best and a loop
**  at worst - so $LOG is a no-op here; the error reaches the caller through
**  the returned status (FAO$K_BADDIR, FAO$K_NOPRM, ...).
*/
#ifdef	$LOG
#undef	$LOG
#endif
#define	$LOG(sts, fmt, ...)	((void) 0)

/*
**  Module-private constants and inline primitives (CRTL replacements)
*/
#define	FAO$K_WIDTHAUTO	(-1)	/* No explicit width given		*/
#define	FAO$K_NUMBUF	32	/* Scratch buffer for numeric conversion*/

#define	$FAO_MAXWIDTH	65535		/* Largest field width / repeat count (the buffer is <= 64K)	*/
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

/* S_FAO$COPY - private memcpy (non-overlapping) */
static	inline	void	s_fao$copy (char * restrict a_dst,
				const char * restrict a_src, size_t a_len)
{
	while ( a_len-- )
		*a_dst++ = *a_src++;
}

/* S_FAO$STRLEN - private strlen */
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

	int		noprm;		/* A parameter was wanted past the end	*/
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

/* S_FAO$SET_STS - record a completion status; the first error wins */
static	inline	void	s_fao$set_sts	(fao_ctx_t *a_ctx, int a_sts)
{
	if ( a_ctx->sts == STS$K_SUCCESS )
		a_ctx->sts = a_sts;
}

/*
**++
**  S_FAO$GET_PRM - fetch the next parameter.
**
**	$FAOL: from the explicit array, past the end yields 0 and sets the NOPRM flag (FAO$K_NOPRM).
**	$FAO: consumed from the va_list on demand and cached, so that !n- can reuse it.
**
**  INPUT:
**	a_prm	- address of the shared parameter state
**
**  RETURN:
**	Value of the parameter
**--
*/
static	fao_prm_t	s_fao$get_prm	(fao_prmstate_t *a_prm)
{
fao_prm_t l_val = 0;

	if ( a_prm->prmlst != NULL )
		{
		if ( a_prm->prmidx < a_prm->prmlstcnt )
			l_val = a_prm->prmlst [a_prm->prmidx];
		else	a_prm->noprm = 1;

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
	else	a_prm->noprm = 1;	/* More than FAO$K_MAXPRM wanted	*/

	a_prm->prmidx++;
	return	l_val;
}

/* S_FAO$PUT_CHR - emit one byte; a full buffer sets FAO$K_BUFOVF and drops the byte */
static	inline	void	s_fao$put_chr	(fao_ctx_t *a_ctx, char a_chr)
{
	if ( a_ctx->optr < a_ctx->oend )
		*a_ctx->optr++ = a_chr;
	else	s_fao$set_sts (a_ctx, FAO$K_BUFOVF);
}

/* S_FAO$PUT_FILL - emit a run of identical bytes, truncated to the room left (FAO$K_BUFOVF) */
static	void	s_fao$put_fill	(fao_ctx_t *a_ctx, char a_chr, size_t a_cnt)
{
size_t	l_room = (size_t) (a_ctx->oend - a_ctx->optr);

	if ( a_cnt > l_room )
		{
		s_fao$set_sts (a_ctx, FAO$K_BUFOVF);
		a_cnt = l_room;
		}

	while ( a_cnt-- )
		*a_ctx->optr++ = a_chr;
}

/* S_FAO$PUT_RAW - emit a byte string in bulk, one bounds check */
static	inline	void	s_fao$put_raw	(fao_ctx_t *a_ctx,
				const char *a_str, size_t a_len)
{
size_t	l_room = (size_t) (a_ctx->oend - a_ctx->optr);

	if ( a_len > l_room )
		{
		s_fao$set_sts (a_ctx, FAO$K_BUFOVF);
		a_len = l_room;
		}

	s_fao$copy (a_ctx->optr, a_str, a_len);
	a_ctx->optr += a_len;
}

/* S_FAO$PUT_STR - emit a string left-justified in a_width (blank-filled, truncated on the right); a_dots: non-printables as '.' */
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

/* S_FAO$CVT_DEC - decimal digits of a_val written backwards from a_end, two per division; returns the first digit */
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
**  S_FAO$PUT_NUM - convert a number and emit it.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_val	- the value; already sign-extended to 64 bits when signed
**	a_base	- radix: 8, 10, 16
**	a_signed- non-zero: signed (decimal only)
**	a_width	- explicit field width or FAO$K_WIDTHAUTO
**	a_defdig- default digit count with zero fill (hex/octal, !Z), 0 - natural width, blank fill
**
**  RETURN:
**	NONE.  Field overflow: decimal is filled with '*', hex/octal is truncated on the left.
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

/* S_FAO$PUT_2D - emit exactly two decimal digits (00..99) */
/* S_FAO$DAYS_TO_CIVIL - UNIX epoch day count to a civil date (proleptic Gregorian) */
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

/* S_FAO$DAYS_FROM_CIVIL - civil date to a UNIX epoch day count (proleptic Gregorian) */
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
**	The sign of the value (seconds) is the absolute/delta discriminator:
**	  > 0	absolute, seconds since 01-JAN-1970 UTC shifted by FAO$K_TZOFFSET, year in full above 9999;
**	  = 0	"no time given" - the current time (the epoch itself is not printable, 1 second after it is);
**	  < 0	delta of -value seconds: "dddd hh:mm:ss.cc" by !%D, "hh:mm:ss.cc" by !%T.
**	An absolute time before the epoch cannot be expressed: a negative value is always a delta.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_prm	- fao_time_t BY VALUE
**	a_date	- non-zero: !%D (dd-MMM-yyyy hh:mm:ss.cc), zero: !%T (hh:mm:ss.cc)
**--
*/
static	void	s_fao$put_time	(fao_ctx_t *a_ctx, fao_prm_t a_prm, int a_date)
{
int64_t	l_now, l_days, l_secs, l_year;
uint64_t l_mag;
int	l_delta;
unsigned l_hh, l_mm, l_ss, l_mon, l_day;
char	l_buf [FAO$K_NUMBUF];
char	*l_dig;

	l_now = (int64_t) a_prm;
	l_delta = (l_now < 0);

	if ( l_delta )				/* Delta time		*/
		{
		l_mag = (uint64_t) 0 - (uint64_t) l_now;	/* Safe for INT64_MIN	*/

		if ( a_date )
			{
			l_dig = s_fao$cvt_dec (l_mag / 86400, l_buf + FAO$K_NUMBUF);
			s_fao$put_raw (a_ctx, l_dig, (size_t) (l_buf + FAO$K_NUMBUF - l_dig));
			s_fao$put_chr (a_ctx, ' ');
			}

		l_secs = (int64_t) (l_mag % 86400);
		l_days = 0;
		}
	else	{
		if ( l_now == 0 )		/* No time given: now	*/
			l_now = (int64_t) FAO$GETTIME ();

		if ( l_now > INT64_MAX - FAO$K_TZOFFSET )	/* Keep the shift defined	*/
			l_now = INT64_MAX - FAO$K_TZOFFSET;

		l_now += FAO$K_TZOFFSET;

		l_days = l_now / 86400;
		l_secs = l_now % 86400;

		if ( l_secs < 0 )
			{
			l_secs += 86400;
			l_days -= 1;
			}
		}

	l_hh = (unsigned) (l_secs / 3600);
	l_mm = (unsigned) ((l_secs / 60) % 60);
	l_ss = (unsigned) (l_secs % 60);

	if ( a_date && (! l_delta) )
		{
		s_fao$days_to_civil (l_days, &l_year, &l_mon, &l_day);

		if ( l_day < 10 )
			s_fao$put_chr (a_ctx, ' ');
		else	s_fao$put_chr (a_ctx, (char)('0' + l_day / 10));

		s_fao$put_chr (a_ctx, (char)('0' + l_day % 10));
		s_fao$put_chr (a_ctx, '-');
		s_fao$put_raw (a_ctx, s_months [l_mon - 1], 3);
		s_fao$put_chr (a_ctx, '-');

		if ( (l_year >= 0) && (l_year <= 9999) )
			{
			s_fao$put_2d  (a_ctx, (unsigned) (l_year / 100));
			s_fao$put_2d  (a_ctx, (unsigned) (l_year % 100));
			}
		else	{			/* Year above 9999: print it in full	*/
			l_dig = s_fao$cvt_dec ((uint64_t) l_year, l_buf + FAO$K_NUMBUF);
			s_fao$put_raw (a_ctx, l_dig, (size_t) (l_buf + FAO$K_NUMBUF - l_dig));
			}

		s_fao$put_chr (a_ctx, ' ');
		}

	s_fao$put_2d  (a_ctx, l_hh);
	s_fao$put_chr (a_ctx, ':');
	s_fao$put_2d  (a_ctx, l_mm);
	s_fao$put_chr (a_ctx, ':');
	s_fao$put_2d  (a_ctx, l_ss);
	s_fao$put_raw (a_ctx, ".00", 3);
}

/* S_FAO$GET_NUMPRM - fetch a B/W/L/Q operand (by value), masked or sign-extended to its size */
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
			return	(uint64_t) l_prm;		/* passed BY VALUE - no dereference */
		}

	return	0;
}

/* S_FAO$FIND_GROUP_END - the "!)"/"!>" matching a group opener (nesting honoured), NULL if none */
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

			if ( *l_dir == 0 )		/* Truncated directive: never step past the NUL */
				return	NULL;

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


static const char s_fao$hexlc[16] = "0123456789abcdef";


/*
**  s_fao$put_hwaddr()/ipv4()/ipv6()/uuid() - format a fixed-length binary
**  address, taken by pointer, into text and emit it through s_fao$put_str().
**  A NULL pointer yields "(null)" rather than a fault. Used by the !%M, !%I,
**  !%6 and !%U directives.
*/
static	void	s_fao$put_hwaddr (				/* aa:bb:cc:dd:ee:ff (6 octets) */
		fao_ctx_t	*a_ctx,
		const void	*a_addr
		)
{
const unsigned char *l_p = a_addr;
char	l_buf [18];
int	l_i, l_n = 0;

	if ( !l_p )
		{
		s_fao$put_str (a_ctx, "(null)", 6, FAO$K_WIDTHAUTO, 0);
		return;
		}

	for ( l_i = 0; l_i < 6; l_i++ )
		{
		if ( l_i )
			l_buf[l_n++] = ':';
		l_buf[l_n++] = s_fao$hexlc[(l_p[l_i] >> 4) & 0xF];
		l_buf[l_n++] = s_fao$hexlc[ l_p[l_i]       & 0xF];
		}
	s_fao$put_str (a_ctx, l_buf, (size_t) l_n, FAO$K_WIDTHAUTO, 0);
}

static	void	s_fao$put_ipv4 (				/* d.d.d.d (4 octets) */
		fao_ctx_t	*a_ctx,
		const void	*a_addr
		)
{
const unsigned char *l_p = a_addr;
char	l_buf [16];
int	l_i, l_n = 0;
unsigned l_v;

	if ( !l_p )
		{
		s_fao$put_str (a_ctx, "(null)", 6, FAO$K_WIDTHAUTO, 0);
		return;
		}

	for ( l_i = 0; l_i < 4; l_i++ )
		{
		if ( l_i )
			l_buf[l_n++] = '.';
		l_v = l_p[l_i];
		if ( l_v >= 100 )
			l_buf[l_n++] = (char)('0' + l_v / 100);
		if ( l_v >= 10 )
			l_buf[l_n++] = (char)('0' + (l_v / 10) % 10);
		l_buf[l_n++] = (char)('0' + l_v % 10);
		}
	s_fao$put_str (a_ctx, l_buf, (size_t) l_n, FAO$K_WIDTHAUTO, 0);
}

static	void	s_fao$put_ipv6 (				/* RFC 5952 canonical form (16 octets) */
		fao_ctx_t	*a_ctx,
		const void	*a_addr
		)
{
const unsigned char *l_p = a_addr;
char	l_buf [46];
int	l_i, l_j, l_n = 0, l_seen;
int	l_run, l_runlen, l_best, l_bestlen;
unsigned l_g [8], l_d;

	if ( !l_p )
		{
		s_fao$put_str (a_ctx, "(null)", 6, FAO$K_WIDTHAUTO, 0);
		return;
		}

	/*
	 * RFC 5952 par.5: an IPv4-mapped address is shown in the mixed notation -
	 * ::ffff:192.168.1.10 - the very form inet_ntop() gives it.
	 */
	for ( l_i = 0; (l_i < 10) && !l_p[l_i]; l_i++);
	if ( (l_i == 10) && (l_p[10] == 0xFF) && (l_p[11] == 0xFF) )
		{
		s_fao$put_str (a_ctx, "::ffff:", 7, FAO$K_WIDTHAUTO, 0);
		s_fao$put_ipv4 (a_ctx, l_p + 12);
		return;
		}

	for ( l_i = 0; l_i < 8; l_i++ )
		l_g[l_i] = ((unsigned) l_p[l_i * 2] << 8) | l_p[l_i * 2 + 1];

	/*
	 * RFC 5952: the longest run of two or more all-zero groups is compressed
	 * to "::"; on a tie the leftmost run wins; a lone zero group is not
	 * compressed. l_best = the run's first group, l_bestlen = its length.
	 */
	l_best = -1; l_bestlen = 1;
	for ( l_i = 0; l_i < 8; )
		{
		if ( l_g[l_i] )
			{
			l_i++;
			continue;
			}
		for ( l_run = l_i, l_runlen = 0; (l_i < 8) && !l_g[l_i]; l_i++ )
			l_runlen++;
		if ( l_runlen > l_bestlen )
			{
			l_best = l_run;
			l_bestlen = l_runlen;
			}
		}

	for ( l_i = 0; l_i < 8; l_i++ )
		{
		if ( (l_best >= 0) && (l_i == l_best) )
			{			/* The compressed run: "::", then continue past it */
			l_buf[l_n++] = ':';
			l_buf[l_n++] = ':';
			l_i += l_bestlen - 1;	/* The for's l_i++ steps onto the group after the run */
			continue;
			}
		if ( l_n && (l_buf[l_n - 1] != ':') )
			l_buf[l_n++] = ':';
		if ( !l_g[l_i] )
			l_buf[l_n++] = '0';
		else	for ( l_j = 12, l_seen = 0; l_j >= 0; l_j -= 4 )
				{
				l_d = (l_g[l_i] >> l_j) & 0xF;
				if ( l_d || l_seen )
					{
					l_buf[l_n++] = s_fao$hexlc[l_d];
					l_seen = 1;
					}
				}
		}
	s_fao$put_str (a_ctx, l_buf, (size_t) l_n, FAO$K_WIDTHAUTO, 0);
}

static	void	s_fao$put_uuid (				/* 8-4-4-4-12 (16 octets) */
		fao_ctx_t	*a_ctx,
		const void	*a_addr
		)
{
const unsigned char *l_p = a_addr;
char	l_buf [37];
int	l_i, l_n = 0;

	if ( !l_p )
		{
		s_fao$put_str (a_ctx, "(null)", 6, FAO$K_WIDTHAUTO, 0);
		return;
		}

	for ( l_i = 0; l_i < 16; l_i++ )
		{
		if ( (l_i == 4) || (l_i == 6) || (l_i == 8) || (l_i == 10) )
			l_buf[l_n++] = '-';
		l_buf[l_n++] = s_fao$hexlc[(l_p[l_i] >> 4) & 0xF];
		l_buf[l_n++] = s_fao$hexlc[ l_p[l_i]       & 0xF];
		}
	s_fao$put_str (a_ctx, l_buf, (size_t) l_n, FAO$K_WIDTHAUTO, 0);
}


/*
**++
**  S_FAO$PROCESS - the FAO engine: scan a control string fragment and emit the output.
**
**  INPUT:
**	a_ctx	- address of the conversion context
**	a_ctl	- address of the control string fragment
**	a_end	- address past the fragment, NULL - up to the terminating zero byte
**	a_depth	- group nesting depth (recursion guard)
**
**  RETURN:
**	Address past the processed fragment; a malformed directive sets FAO$K_BADDIR in the context
**	and processing goes on from the next byte.
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
char	l_dir, l_size, l_chr;
char	l_fldbuf [FAO$K_FLDBUF];

#define	$FAO_MORE()	( (a_end == NULL) ? (*l_ptr != 0) : (l_ptr < a_end) )
#define	$FAO_NEXT()	( $FAO_MORE () ? *l_ptr++ : 0 )		/* Next byte, or 0 at the end - never past it	*/

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
		if ( $FAO_MORE () && (*l_ptr == '#') )
			{
			l_val = (uint32_t) s_fao$get_prm (l_prm);
			l_width = (l_val > $FAO_MAXWIDTH) ? $FAO_MAXWIDTH : (int) l_val;
			l_ptr++;
			}
		else if ( $FAO_MORE () && $FAO_ISDIGIT (*l_ptr) )
			{
			l_width = 0;

			do	{
				if ( l_width < $FAO_MAXWIDTH )		/* Clamp: no int overflow	*/
					l_width = l_width * 10 + (*l_ptr - '0');
				l_ptr++;
				} while ( $FAO_MORE () && $FAO_ISDIGIT (*l_ptr) );

			if ( l_width > $FAO_MAXWIDTH )
				l_width = $FAO_MAXWIDTH;
			}

		l_dir = $FAO_NEXT ();

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
				switch ( $FAO_NEXT () )
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

					case 'M':	/* !%M - MAC / hardware address	*/
						s_fao$put_hwaddr (a_ctx,
							(const void *)(uintptr_t) s_fao$get_prm (l_prm));
						break;

					case 'I':	/* !%I - IPv4 address		*/
						s_fao$put_ipv4 (a_ctx,
							(const void *)(uintptr_t) s_fao$get_prm (l_prm));
						break;

					case '6':	/* !%6 - IPv6 address		*/
						s_fao$put_ipv6 (a_ctx,
							(const void *)(uintptr_t) s_fao$get_prm (l_prm));
						break;

					case 'U':	/* !%U - UUID			*/
						s_fao$put_uuid (a_ctx,
							(const void *)(uintptr_t) s_fao$get_prm (l_prm));
						break;

					default:
						s_fao$set_sts (a_ctx, FAO$K_BADDIR);
						$LOG (STS$K_ERROR,
							"Bad !%% directive at offset %ld",
							(long) (l_ptr - a_ctl));
						break;
					}
				break;

			case '*':		/* !n*c - repeat char	*/
				l_chr = $FAO_NEXT ();

				if ( l_chr == 0 )	/* "!n*" with no character	*/
					{
					s_fao$set_sts (a_ctx, FAO$K_BADDIR);
					$LOG (STS$K_ERROR, "Truncated !n* directive");
					break;
					}

				s_fao$put_fill (a_ctx, l_chr,
					(l_width == FAO$K_WIDTHAUTO)
						? 1 : (size_t) l_width);
				break;

			case '(':		/* !n(...) - repeat group */
				{
				l_grpend = s_fao$find_group_end (l_ptr, '(', ')');

				if ( (l_grpend == NULL)
					|| (a_depth >= FAO$K_MAXDEPTH) )
					{
					s_fao$set_sts (a_ctx, FAO$K_BADDIR);
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
					s_fao$set_sts (a_ctx, FAO$K_BADDIR);
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

				if ( l_sub.sts != STS$K_SUCCESS )	/* Field overflow is a field cut, not a buffer cut	*/
					s_fao$set_sts (a_ctx, (l_sub.sts == FAO$K_BUFOVF)
						? FAO$K_FLDOVF : l_sub.sts);

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
				switch ( $FAO_NEXT () )
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
						s_fao$set_sts (a_ctx, FAO$K_BADDIR);
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
				l_size	= $FAO_NEXT ();
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
						s_fao$set_sts (a_ctx, FAO$K_BADDIR);
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

			case 0:			/* "!", "!5": the string ended inside a directive */
				s_fao$set_sts (a_ctx, FAO$K_BADDIR);
				$LOG (STS$K_ERROR, "Truncated FAO directive");
				break;

			default:
				s_fao$set_sts (a_ctx, FAO$K_BADDIR);
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
**  UTIL$FAO - portable SYS$FAO, varargs form.
**
**	NOTE: every variadic parameter MUST be cast by the caller to fao_prm_t (64 bits); a quadword and a
**	time are passed BY VALUE, only the string and address directives take a pointer.  This form has no
**	parameter count and cannot detect too few arguments: use __util$faol() (or $LOG_FAO/$PUTMSG_FAO,
**	which count) when the control string is not a literal of the caller.
**
**  INPUT:
**	a_ctlstr - zero-terminated control string
**	a_outlen - optional address of a word to receive the result length, may be NULL
**	a_outbuf - output buffer descriptor (fao_desc_t), dsc$w_length is the capacity on input
**	...	 - conversion parameters, each of type fao_prm_t
**
**  OUTPUT:
**	Buffer of a_outbuf - the result (not zero-terminated); *a_outlen - its length
**
**  RETURN:
**	STS$K_SUCCESS, or the first error found (the text is produced anyway):
**	  FAO$K_BUFOVF - output truncated to the capacity
**	  FAO$K_BADDIR - unknown, malformed or truncated directive
**	  FAO$K_FLDOVF - a !n<...!> field exceeded FAO$K_FLDBUF bytes and was cut
**	  FAO$K_NOPRM  - more than FAO$K_MAXPRM parameters wanted
**	  STS$K_ERROR  - bad routine arguments
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
	l_prm.noprm	= 0;
	l_prm.lastnum	= 0;

	l_ctx.obase	= a_outbuf->dsc$a_pointer;
	l_ctx.optr	= l_ctx.obase;
	l_ctx.oend	= l_ctx.obase + a_outbuf->dsc$w_length;
	l_ctx.prm	= &l_prm;
	l_ctx.sts	= STS$K_SUCCESS;

	(void) s_fao$process (&l_ctx, a_ctlstr, NULL, 0);

	va_end (l_ap);

	if ( l_prm.noprm )
		s_fao$set_sts (&l_ctx, FAO$K_NOPRM);

	if ( a_outlen != NULL )
		*a_outlen = (unsigned short) (l_ctx.optr - l_ctx.obase);

	return	l_ctx.sts;
}

/*
**++
**  UTIL$FAOL - portable SYS$FAOL, list form.
**
**  INPUT:
**	a_ctlstr - zero-terminated control string
**	a_outlen - optional address of a word to receive the result length, may be NULL
**	a_outbuf - output buffer descriptor (fao_desc_t)
**	a_prmlst - parameter array (fao_prm_t elements)
**	a_prmcnt - element count (explicit, unlike OpenVMS); a directive past it gets 0
**
**  OUTPUT:
**	Buffer of a_outbuf - the result; *a_outlen - its length
**
**  RETURN:
**	As UTIL$FAO; FAO$K_NOPRM when the control string wanted more than a_prmcnt parameters.
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
	l_prm.noprm	= 0;
	l_prm.lastnum	= 0;

	l_ctx.obase	= a_outbuf->dsc$a_pointer;
	l_ctx.optr	= l_ctx.obase;
	l_ctx.oend	= l_ctx.obase + a_outbuf->dsc$w_length;
	l_ctx.prm	= &l_prm;
	l_ctx.sts	= STS$K_SUCCESS;

	(void) s_fao$process (&l_ctx, a_ctlstr, NULL, 0);

	if ( l_prm.noprm )			/* The control string wanted more than a_prmcnt	*/
		s_fao$set_sts (&l_ctx, FAO$K_NOPRM);

	if ( a_outlen != NULL )
		*a_outlen = (unsigned short) (l_ctx.optr - l_ctx.obase);

	return	l_ctx.sts;
}

/* S_FAO$TIM_NUM - parse an unsigned decimal of the time string; returns the digit count (0 = field omitted) */
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

/* S_FAO$TIM_HMS - parse "hh:mm[:ss[.cc]]", fields may be omitted ("::10"); FAO$K_IVTIME on a range violation */
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

/* S_FAO$TIM_MONTH - match a case-insensitive three-letter month name; FAO$K_IVTIME if unknown */
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
**  UTIL$BINTIM - portable SYS$BINTIM: ASCII time string to binary time.
**
**	The result is fao_time_t (seconds of the UNIX epoch), the very value !%T/!%D consume.  Hundredths are
**	parsed and validated but dropped.  Blanks around the string are ignored.  Accepted forms:
**	  "dd-MMM-yyyy[ hh:mm[:ss[.cc]]]"	absolute, in the FAO$K_TZOFFSET timezone; omitted date fields
**						default to the current date, omitted time to 00:00:00.  A date before
**						01-JAN-1970 gives FAO$K_IVTIME; the epoch itself gives 0 ("now" to the display routines)
**	  "hh:mm[:ss[.cc]]"			absolute, the current date
**	  "dddd hh:mm[:ss[.cc]]" or "dddd-hh:mm..."	delta: a NEGATIVE result
**	Time-of-day fields may be omitted individually ("::10" is ten seconds).
**
**  INPUT:
**	a_timstr - zero-terminated time string
**	a_timadr - address of the fao_time_t to receive the result
**
**  OUTPUT:
**	*a_timadr - binary time; unchanged on error
**
**  RETURN:
**	STS$K_SUCCESS; FAO$K_IVTIME - syntax error or field out of range; STS$K_ERROR - bad routine arguments
**--
*/
int	__util$bintim	(const char *a_timstr, fao_time_t *a_timadr)
{
const char *l_ptr = a_timstr;
unsigned l_num = 0, l_mon, l_secs = 0;
unsigned l_day, l_cmon, l_cday, l_max;
int64_t	l_year, l_cyear, l_now, l_abs;
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

		if ( !(1 & (l_sts = s_fao$tim_hms (&l_ptr, &l_secs))) )
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

		if ( !(1 & (l_sts = s_fao$tim_month (&l_ptr, &l_mon))) )
			return	l_sts;

		if ( *l_ptr++ != '-' )
			return	FAO$K_IVTIME;

		if ( s_fao$tim_num (&l_ptr, &l_num) )
			l_year = (int64_t) l_num;

		while ( (*l_ptr == ' ') || (*l_ptr == '\t') )
			l_ptr++;

		if ( *l_ptr )
			if ( !(1 & (l_sts = s_fao$tim_hms (&l_ptr, &l_secs))) )
				return	l_sts;
		}
	else if ( l_numdig || (*l_ptr == ':') )
		{
		/* Absolute, time only: today at hh:mm[:ss[.cc]] */
		l_ptr = a_timstr;

		while ( (*l_ptr == ' ') || (*l_ptr == '\t') )
			l_ptr++;

		if ( !(1 & (l_sts = s_fao$tim_hms (&l_ptr, &l_secs))) )
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

	l_abs = s_fao$days_from_civil (l_year, l_mon, l_day) * 86400
		+ (int64_t) l_secs - FAO$K_TZOFFSET;

	/*
	** A negative binary time is a delta, so an absolute time before the
	** epoch has no representation; refuse it rather than hand back a
	** value that every consumer would read as a delta.
	*/
	if ( l_abs < 0 )
		return	FAO$K_IVTIME;

	*a_timadr = (fao_time_t) l_abs;

	return	STS$K_SUCCESS;
}

/*
**++
**  UTIL$GETTIM - portable SYS$GETTIM: the current binary time.
**
**  INPUT:
**	a_timadr - address of the fao_time_t to receive the result
**
**  OUTPUT:
**	*a_timadr - seconds of the UNIX epoch (UTC)
**
**  RETURN:
**	STS$K_SUCCESS; STS$K_ERROR - a_timadr is NULL
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
**  UTIL$ASCTIM - portable SYS$ASCTIM: binary time to an ASCII string.
**
**	The renderer is that of !%D/!%T, and UTIL$BINTIM accepts the result back.  Absolute (> 0, in the
**	FAO$K_TZOFFSET timezone): "dd-MMM-yyyy hh:mm:ss.00", or "hh:mm:ss.00" with a_cvtflg = 1.  Delta (< 0):
**	"dddd hh:mm:ss.00", or "hh:mm:ss.00" with a_cvtflg = 1.  Zero is "no time given" and means now, as a
**	missing time address does; an absolute time before the epoch cannot be expressed.
**
**  INPUT:
**	a_outlen - optional address of a word to receive the result length, may be NULL
**	a_outbuf - output buffer descriptor (fao_desc_t)
**	a_timadr - address of the binary time; NULL - the current time
**	a_cvtflg - 0 - full, 1 - time of day only
**
**  OUTPUT:
**	Buffer of a_outbuf - the result (not zero-terminated); *a_outlen - its length
**
**  RETURN:
**	STS$K_SUCCESS; FAO$K_BUFOVF - truncated to the capacity; STS$K_ERROR - bad routine arguments
**--
*/
int	__util$asctim	(unsigned short *a_outlen, fao_desc_t *a_outbuf,
			const fao_time_t *a_timadr, int a_cvtflg)
{
fao_ctx_t l_ctx;
fao_prmstate_t l_prm = {0};
fao_time_t l_tim;

	if ( (a_outbuf == NULL) || (a_outbuf->dsc$a_pointer == NULL) )
		return	STS$K_ERROR;

	if ( a_timadr != NULL )
		l_tim = *a_timadr;
	else	l_tim = (fao_time_t) FAO$GETTIME ();

	l_ctx.obase	= a_outbuf->dsc$a_pointer;
	l_ctx.optr	= l_ctx.obase;
	l_ctx.oend	= l_ctx.obase + a_outbuf->dsc$w_length;
	l_ctx.prm	= &l_prm;
	l_ctx.sts	= STS$K_SUCCESS;

	/* The same renderer as !%D/!%T, so the two can never disagree (by value; sign-extended) */
	s_fao$put_time (&l_ctx, (fao_prm_t) (int64_t) l_tim, ! a_cvtflg);

	if ( a_outlen != NULL )
		*a_outlen = (unsigned short) (l_ctx.optr - l_ctx.obase);

	return	l_ctx.sts;
}

/*
**++
**  UTIL$ADD_TIMES - portable LIB$ADD_TIMES: add two binary times.
**
**	delta + delta -> delta; absolute + delta -> absolute; absolute + absolute is an error.
**
**  INPUT:
**	a_tim1, a_tim2	- addresses of the operands
**
**  OUTPUT:
**	*a_result - the sum; unchanged on error
**
**  RETURN:
**	STS$K_SUCCESS; FAO$K_IVTIME - both absolute; STS$K_ERROR - bad routine arguments
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
**  UTIL$SUB_TIMES - portable LIB$SUB_TIMES: subtract the second binary time from the first.
**
**	absolute - absolute -> delta (tim1 must not precede tim2); absolute - delta -> absolute (not negative);
**	delta - delta -> delta (|tim1| >= |tim2|); delta - absolute is an error.
**
**  INPUT:
**	a_tim1	- address of the minuend
**	a_tim2	- address of the subtrahend
**
**  OUTPUT:
**	*a_result - the difference; unchanged on error
**
**  RETURN:
**	STS$K_SUCCESS; FAO$K_NEGTIM - result would be negative; FAO$K_IVTIME - delta minus absolute;
**	STS$K_ERROR - bad routine arguments
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
