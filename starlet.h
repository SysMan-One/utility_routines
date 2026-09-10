/*
**++
**
**  FACILITY:	STARLET - Portable analogues of OpenVMS system services
**
**  MODULE:	starlet.h
**
**  ABSTRACT:	Public definitions and prototypes of the STARLET module:
**		portable implementations of OpenVMS system services.
**		Currently: UTIL$FAO, UTIL$FAOL, UTIL$BINTIM, UTIL$GETTIM,
**		UTIL$ASCTIM, UTIL$ADD_TIMES, UTIL$SUB_TIMES; further
**		analogues are to be accumulated here.
**
**  AUTHOR:	Claude (Anthropic) for D.Novikov
**
**  CREATION DATE:  24-JUL-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-03		24-JUL-2026	CLAUDE
**		Prototypes of UTIL$GETTIM, UTIL$ASCTIM, UTIL$ADD_TIMES,
**		UTIL$SUB_TIMES added; FAO$K_NEGTIM added.
**
**	X.01-02		24-JUL-2026	CLAUDE
**		Module renamed fao.h -> starlet.h (facility STARLET).
**		No functional changes.
**
**	X.01-01		24-JUL-2026	CLAUDE
**		Routine prefix changed sys$ -> util$; fao_time_t moved
**		here from fao.c; UTIL$BINTIM added; FAO$K_IVTIME added.
**
**	X.01-00		24-JUL-2026	CLAUDE
**		Initial version.
**
**--
*/

#ifndef	__STARLET$H__
#define	__STARLET$H__	1

#include	<stddef.h>
#include	<stdint.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
**  Fallback definitions of the status codes, normally supplied by the
**  UTIL$ module (utility_routines.h).  Guarded to avoid redefinition.
*/
#ifndef	STS$K_SUCCESS
#define	STS$K_WARN	0
#define	STS$K_SUCCESS	1
#define	STS$K_ERROR	2
#define	STS$K_INFO	3
#define	STS$K_FATAL	4
#endif

/*
**  Additional FAO-specific completion codes (module-local convention):
**  the routines return STS$K_SUCCESS on full conversion, FAO$K_BUFOVF
**  when output has been truncated, FAO$K_BADDIR on malformed directive.
*/
#define	FAO$K_BUFOVF	0xa	/* Output buffer overflow, result truncated */
#define	FAO$K_BADDIR	6	/* Unknown or malformed FAO directive */
#define	FAO$K_IVTIME	0xc	/* Invalid time string (UTIL$BINTIM)	*/
#define	FAO$K_NEGTIM	8	/* Negative time result (LIB$_NEGTIM)	*/

/*
**  Limits
*/
#define	FAO$K_MAXPRM	64	/* Max number of cached parameters ($FAO)   */
#define	FAO$K_MAXDEPTH	4	/* Max nesting depth of !n( ) / !n< > groups*/

/*
**  Portable analogue of the OpenVMS string descriptor
**  (struct dsc$descriptor_s).  Only the fields actually used by
**  the FAO engine are declared.
*/
typedef	struct	__fao_desc
{
	unsigned short	dsc$w_length;	/* Buffer capacity, bytes	    */
	unsigned char	dsc$b_dtype;	/* Not interpreted, kept for layout */
	unsigned char	dsc$b_class;	/* Not interpreted, kept for layout */
	char		*dsc$a_pointer;	/* Address of the buffer	    */
} fao_desc_t;

/*
**  Static initializer for a descriptor over a char array:
**	char buf[256];
**	fao_desc_t dsc = $FAO_DESC(buf);
*/
#define	$FAO_DESC(buf)	{ (unsigned short) sizeof(buf), 0, 0, (buf) }

/*
**  Type of a single FAO parameter.  On OpenVMS every argument of $FAO
**  is a longword; in the portable variant it is a 64-bit value able to
**  hold a 64-bit integer, a time value or a pointer - all BY VALUE.  A
**  quadword (!xQ) and a time (!%D/!%T) are therefore passed by value, not
**  by reference.  Callers of __util$fao() MUST cast every parameter to
**  fao_prm_t explicitly (e.g. (fao_prm_t) my_uint64, (fao_prm_t) my_time).
*/
typedef	uint64_t	fao_prm_t;

/*
**  Binary time type used by !%T/!%D and UTIL$BINTIM: seconds of the
**  UNIX epoch.  Defaults to time_t of the hosted CRTL; a freestanding
**  build overrides it by defining FAO$T_TIME (typically int64_t)
**  before including this header (and FAO$GETTIME before starlet.c).
*/
#ifndef	FAO$T_TIME
#include	<time.h>
#define	FAO$T_TIME	time_t
#endif

typedef	FAO$T_TIME	fao_time_t;

/*
**  Public routines, see starlet.c for the detailed description of the
**  supported directives and of the UTIL$BINTIM time string grammar.
*/
int	__util$fao	(const char *a_ctlstr, unsigned short *a_outlen,
			fao_desc_t *a_outbuf, ...);

int	__util$faol	(const char *a_ctlstr, unsigned short *a_outlen,
			fao_desc_t *a_outbuf,
			const fao_prm_t *a_prmlst, int a_prmcnt);

int	__util$bintim	(const char *a_timstr, fao_time_t *a_timadr);

int	__util$gettim	(fao_time_t *a_timadr);

int	__util$asctim	(unsigned short *a_outlen, fao_desc_t *a_outbuf,
			const fao_time_t *a_timadr, int a_cvtflg);

int	__util$add_times	(const fao_time_t *a_tim1, const fao_time_t *a_tim2,
			fao_time_t *a_result);

int	__util$sub_times	(const fao_time_t *a_tim1, const fao_time_t *a_tim2,
			fao_time_t *a_result);

#ifdef	__cplusplus
}
#endif

#endif	/* __STARLET$H__ */
