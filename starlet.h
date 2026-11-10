/*
**++
**
**  FACILITY:	STARLET - Portable analogues of OpenVMS system services
**
**  MODULE:	starlet.h
**
**  ABSTRACT:	Public definitions and prototypes of portable analogues of OpenVMS
**		system services: $FAO/$FAOL, $BINTIM, $GETTIM, $ASCTIM, $ADD_TIMES,
**		$SUB_TIMES (as __util$...), their completion codes and types.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  24-JUL-2026
**
**  IDENT:	X.01-04		REV: 1.4.0
**
**  MODIFICATION HISTORY:
**
**	X.01-04		31-OCT-2026	CLD
**		StarLet 1.7.0: includes utility_routines.h itself (include order no longer matters);
**		FAO$K_* are real condition values (BADDIR 6 -> 0x12; NOPRM, FLDOVF added).
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
**  STS$K_* are enumerators of utility_routines.h: pulled in here, so the
**  include order of the two headers does not matter.
*/
#include	"utility_routines.h"

/*
**  FAO and binary-time completion codes: OpenVMS-layout condition values
**  (severity in bits 0-2, message number in bits 3-15, no facility), all
**  even, so only STS$K_SUCCESS (1) is a success.  Numbers are stable from
**  1.7.0 on, except FAO$K_BADDIR (was 6).
*/
#define	FAO$K_BUFOVF	0x0a	/* Output buffer overflow, result truncated */
#define	FAO$K_BADDIR	0x12	/* Unknown, malformed or truncated directive */
#define	FAO$K_IVTIME	0x0c	/* Invalid time string (UTIL$BINTIM)	*/
#define	FAO$K_NEGTIM	0x08	/* Negative time result (LIB$_NEGTIM)	*/
#define	FAO$K_NOPRM	0x1a	/* Too few parameters for the control string */
#define	FAO$K_FLDBUF	512	/* The longest !n<...!> field text			*/
#define	FAO$K_FLDOVF	0x22	/* !n<...!> field text longer than FAO$K_FLDBUF, cut */

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
**  A single FAO parameter: a 64-bit value passed BY VALUE (integer, quadword,
**  time, pointer).  Time 0 means "now", a negative one is a delta.  Callers of
**  __util$fao() MUST cast every parameter to fao_prm_t.
*/
typedef	uint64_t	fao_prm_t;

/*
**  Binary time type of !%T/!%D and UTIL$BINTIM: seconds of the UNIX epoch,
**  time_t by default; a freestanding build defines FAO$T_TIME (and FAO$GETTIME).
*/
#ifndef	FAO$T_TIME
#include	<time.h>
#define	FAO$T_TIME	time_t
#endif

typedef	FAO$T_TIME	fao_time_t;

/*
**  Public routines, see starlet.c for the directives and the time string grammar.
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
