#ifdef _WIN32
	#pragma once
	#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif

/*
**  Abstract: An API declaration for the general purpose and utility routines: queues, ILE, messages and logging, string and time helpers.
**
**  Author: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  Creation date: 4-SEP-2017
**
**  Modification history:
**
**	14-SEP-2017	RRL	Added __util$iszero() inline routine.
**	22-SEP-2017	RRL	Added __util$hex2bin() inline routine.
**	26-SEP-2017	RRL	Added __util$strcat_rx - concatenates given list of strings into the single area.
**	 5-OCT-2017	RRL	Added __util$syslog() routine declaration;
**				changed __util$deflog() routine declaration;
**
**	20-OCT-2017	RRL	Added __util$uuid2str() - convert UUID into from binary form to ASCII text string.
**
**	27-OCT-2017	RRL	Added __util$movc5 - copy source string to destionation place, fill rest of output
**				buffer with a given character.
**
**	14-NOV-2017	RRL	Added __util$str2asc() - copy ASCIZ string to the ASCIC container.
**
**	11-MAY-2018	RRL	Added __util$fill() - fill buffer with the given pattern data
**
**	30-MAY-2018	RRL	Correct pointer type to exclude compilation error under VC 2017 in the __util$fill().
**
**	 7-JUN-2018	RRL	Correct pointer type to compile under GCC.
**
**	15-OCT-2018	RRL	Added __ASC_TYPE__ macro to flag that ASC type is defined here.
**
**	25-JUN-2019	RRL	Redefined macro $TRACE() to eliminate "if (cond) ..."
**
**	10-JUL-2019	RRL	Warning fix.
**
**	17-MAR-2021	RRL	Added a set of data structures EMSG_* to support $INIMSG/$PUTMSG/$GETMSG routines.
**
**	28-SEP-2021	Anton	Added changes to fix compilation reports;
**				added an reference to an external logging handler routine.
**
**	19-MAR-2023	RRL	Added int __util$uint32_2_dec()
**
**	26-JUN-2023	RRL	Revised and refactored __util$lookup_key
**
**	 9-OCT-2023	RRL	Revised and refactored __util$lookup_key
**
**	10-OCT-2024	RRL	Fix $IFTRACE() in Release compilation
**
**	10-SEP-2026	RRL	Added __util$base64() / __util$unbase64() - RFC 4648 encoding and
**				decoding, bounded buffers, (src, srclen, dst, dstsz, &dstlen).
**
**	11-SEP-2026	RRL	$MSG() did not mask the facility off, so it was not the inverse of
**				$DEFMSG(): for a non-zero facility it returned <facility> << 13 |
**				<number>. It now extracts the 13-bit field of the layout comment.
**
**	 4-OCT-2026	RRL	Added __util$rotatelogfile() - rotation of the log file opened by
**				__util$deflog() to <file>.1 ... <file>.<keep> past a size limit.
**
**	22-OCT-2026	RRL	Windows (MinGW-w64 cross build): <windows.h> in lower case; the time
**				routine is s___time() as on POSIX (clock_gettime() under MinGW-w64,
**				timespec_get() otherwise), tv_nsec no longer divided by 1024.
**
**	31-OCT-2026	CLD	__util$lookup_key: UTIL$K_LOOKUP_ABBREV fixed (past-the-match, shorter keyword never compared);
**				a full spelling wins, an abbreviation must select one keyword, else STS$K_WARN.
**
**	31-OCT-2026	CLD	1.7.1 : __util$hex2bin() validates its input (-1, nothing written); OPTS$K_INT signedness documented.
**
**	31-OCT-2026	CLD	1.7.0 : defects fixed in the queue routines (movqtail/movqhead, empty-queue remq*, UTIL$S_NOLOCK),
**				$ILE3INI, bzero/iszero, strelem, str2asc, isinrange2, cmp_time/sub_time, uncomment, collapse,
**				strcat_rx, mask2sym, fill; __util$readparams no longer declared; __util$movc5 rewritten.
**
*/

#if _WIN32
#define _CRT_SECURE_NO_WARNINGS	1

#include	<windows.h>		/* lower case: MinGW-w64 cross builds live on a case-sensitive file system */
#include	<sys/timeb.h>

#endif

#ifndef	__STARLET$UTILS__
#define __STARLET$UTILS__	1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	__MODULE__
#define	__MODULE__	NULL
#endif

#include	<stdio.h>
#include	<stddef.h>
#include	<assert.h>
#include	<time.h>
#include	<ctype.h>
#include	<limits.h>
#include	<stdarg.h>
#include	<string.h>



#ifndef	WIN32
	#include	<pthread.h>
	#include	<unistd.h>
#endif

#define	CRLFCRLF_LW	0x0a0d0a0d
#define	CRLFCRLF	"\r\n\r\n"
#define	CRLF		"\r\n"

#ifdef		WIN32
	#define likely(x)       (x)
	#define unlikely(x)     (x)
#else
#ifndef	likely
	#define likely(x)       __builtin_expect((x),1)
#endif

#ifndef	unlikely
	#define unlikely(x)     __builtin_expect((x),0)
#endif

#endif


/*
 * For bagent logging
*/




#ifndef	WIN32

	#ifndef	_GNU_SOURCE
		#define	_GNU_SOURCE	1
	#endif

#include		<sys/syscall.h>

#if !defined(ANDROID) && !defined(Q_OS_LINUX)



#if	0 //_REENTRANT
static inline pid_t	gettid(void)
{

pthread_t	tid = 0;


	tid = pthread_self();
	tid = tid < 0 ? getpid() : tid;

	return	-tid;
}
#else


static inline pid_t	__gettid(void)
{
	return	syscall (
#if defined(__APPLE__) || defined(__OSX__)
	SYS_thread_selfid
#else
	SYS_gettid
#endif	/* ANDROID && LINUX*/
	);
}

#endif	/* #if !defined(ANDROID) && !defined(Q_OS_LINUX) */
#endif	/* #ifndef	WIN32 */

#endif











/*
 * A Macro to compute an arrays size in elements at compilation time
 */
#define	$ARRSZ(a)	(sizeof(a)/sizeof(a[0]))


/* Atomic compare-and-swap of an int, 1 if the new value was stored (GCC __sync_bool_compare_and_swap) */
#define	$CMP_STORE_LONG(src, old_val, new_val)	__sync_bool_compare_and_swap((int *) src, (int) old_val, (int) new_val )


#pragma	pack	(push)
#pragma	pack	(1)


#if _WIN32
#define inline	__inline
#endif

#ifdef _WIN32
/*
struct	timespec {
	time_t	tv_sec;
	long	tv_nsec;
};
*/
inline static	int	s___time(struct timespec *tv)		/* the same name as the POSIX one below - __util$log() calls s___time() */
{

#if    1

#if defined(__MINGW32__)
	clock_gettime(CLOCK_REALTIME, tv);	/* MinGW-w64 (winpthreads) has it, its <time.h> has no TIME_UTC */
#else
	timespec_get(tv, TIME_UTC);
#endif
	//tv->tv_nsec *=1024;
#else
struct _timeb timebuffer;

	_ftime_s ( &timebuffer );

	tv->tv_sec = timebuffer.time;
	tv->tv_nsec = timebuffer.millitm;
	tv->tv_nsec *= 1024;


#endif // 1

	return	0;
}

#else

inline static int	s___time(struct timespec *tv)
{
	clock_gettime(CLOCK_REALTIME_COARSE, tv);

	return	0;
}

#endif




inline	static void	__util$timbuf(time_t *__src, struct tm * __dst)
{
time_t	now;

	if ( !__src )
		time(&now);

#ifdef	WIN32
	localtime_s(__dst, __src ? __src : &now);
#else
	localtime_r(__src ? __src : &now, __dst);
#endif

	__dst->tm_mon	+= 1;
	__dst->tm_year	+= 1900;
}


#ifndef	__PATH_SEPARATOR
	#ifndef	WIN32
		#define	__PATH_SEPARATOR	'/'
	#else
		#define	__PATH_SEPARATOR	'\\'
	#endif
#endif


#ifndef	__PATH_SEPARATOR_S
	#ifndef	WIN32
		#define	__PATH_SEPARATOR_S	"/"
	#else
		#define	__PATH_SEPARATOR_S	"\\"
	#endif
#endif

/*
 *
 * Item List Element
 *
 */
typedef	struct	__ile2	{
	unsigned short	code,	/* Item List Element code	*/
			len;	/* Length of data		*/
		char *	ptr;	/* Pointer to data		*/
} ILE2;

/*
** NB: this is NOT the layout of the OpenVMS ILE3, which begins with the length word:
**
**	OpenVMS ILE3	: length (word), code (word), address, return length address
**	this ILE3	: code (word), length (word), address, return length address
**
** Both words are 16 bits, so the size is the same, but a list built here must not be handed to a
** VMS system service as it stands.
*/
typedef	struct	__ile3	{
	unsigned short	code,	/* Item List Element code	*/
			len;	/* A size of the buffer		*/
		char *	ptr;	/* An address of buffer to accept data */
	unsigned short * retlen;/* An actual length of the data returned in the buffer */
} ILE3;


/* Macros to initialize Item List with a given values */
#define	$ILE2INI(code, len, ptr)		{code, len, (char *)ptr}
/* Fills the ILE3 object <a_ile> (not a pointer to it), as a statement: $ILE3INI(l_ile, 1, 4, &l_val, &l_retlen); */
#define	$ILE3INI(a_ile, a_code, a_len, a_ptr, a_retlen)	do {			\
		(a_ile).code = (unsigned short) (a_code);			\
		(a_ile).len = (unsigned short) (a_len);			\
		(a_ile).ptr = (char *) (a_ptr);				\
		(a_ile).retlen = (a_retlen);					\
	} while (0)
#define	$ILENULL	{0}

/*
 *
 * Queue & Entry types & macros definitions
 *
 */
#pragma	pack (push)
#pragma	pack	(8)

/* Links part to embed as the first member of a structure to be queued by the $INSQ/$REMQ macros */
typedef	struct	__entry	{
	struct	__entry *left,	/* References (address) to left and right elements in the queue */
			*right;	/* NULL if the element is first or last in the queue		*/
	void	*	queue;	/* A link to __QUEUE structure					*/
} ENTRY;


/*
** Special data type to help organize of duble-linked lists (queues).
*/
typedef	struct	__queue	{
	ENTRY		*head, *tail;	/* An addresses of first and last element of the queue */

#if _WIN32
	SRWLOCK		lock;		/* A spinlock to coordinate an access to the head and tail*/
#else
	int		lock;
#endif

	unsigned	count;		/* An actual elements/entries count in the queue	*/
} __QUEUE;

/* Macro to initialize a __QUEUE object with defaults. Typical usage is:
 * void ZZtop (void)
 * {
 * __QUEUE que = QUEUE_INITIALIZER;
 *  ...
 * }
 */
#define	QUEUE_INITIALIZER { (ENTRY *) 0, (ENTRY *) 0, 0, 0 }

#pragma	pack	(pop)


/* Logging facility	*/
#ifndef	__FAC__
	#define	__FAC__	"UTILS"
	#define	__FAC$_UTILS__	0x01
#endif


enum	{
	STS$K_WARN	= 0,
	STS$K_SUCCESS	= 1,
	STS$K_ERROR	= 2,
	STS$K_INFO	= 3,
	STS$K_FATAL	= 4,
	STS$K_UNDEF	= 5,
	STS$K_UNDEF5	= 5,
	STS$K_UNDEF6	= 6,
	STS$K_UNDEF7	= 7,


	STS$K_MAX	= 8			/* End-Of-List */
};

#define	STS$M_SYSLOG	16			/* This option force sending message to the syslog service	*/


#define	STS$C__WARN	"W"
#define	STS$C__SUCCESS	"S"
#define	STS$C__ERROR	"E"
#define	STS$C__INFO	"I"
#define	STS$C__FATAL	"F"
#define	STS$C__UNDEF	"?"


#define	$STS_ERROR(code)	( ($SEV(code) == STS$K_ERROR) || ($SEV(code) == STS$K_FATAL) )

/*
 * +---------------------+--------------------------+-------------------+
 * |  Facility (16 bits) | Message Number (13 bits) | Severity (3) bits |
 * +---------------------+--------------------------+-------------------+
 */
#define	$FAC(code)		((code) >> 16)
#define	$MSG(code)		( ((code) >> 3) & 0x00001FFFUL )		/* message number <- bits 3..15	*/
#define	$SEV(code)		((code) & 0x7)


#define	$DEFFAC(code)		((code) << 16)
#define	$DEFMSG(code)		( ((code) << 3) & 0x0000FFF8UL )		/* message number -> bits 3..15 */
#define	$DEFSEV(code)		((code) & 0x7)



typedef	struct __emsg_record__	{
		//EMSG_STS	sts;
		int		sts;				/* Message number, LONGWORD*/

#pragma pack(push, 1)
		struct {
		unsigned char	textl;				/* Message FAO, ASCIC */
			char	text[254];
		};
#pragma pack(pop)

		const char	*facname;			/* Facility name, stamped by __util$inimsg()
								** from the descriptor - a record answers for
								** its %<facility>- envelope with no list walk
								*/

		const char	*mnemonic;			/* The real VMS mnemonic, e.g. "CONFOK" - a
								** $XXX_MSGENT table supplies it stringised from
								** its own "FAC$_NAME" code (#a_sts); __util$inimsg()
								** trims it down to the part after "$_". NULL for a
								** caller-built EMSG_RECORD that never set it - the
								** %<facility>-<sev>- envelope is then left exactly
								** as it was before this field existed.
								*/
} EMSG_RECORD;



typedef struct __emsg_record_desc__ {
		struct __emsg_record_desc__	*link;		/* A link to next Message Descriptor */
		unsigned		msgnr;			/* A number of messages in the msgs[] */
		unsigned		facno;			/* A facility number */
		struct __emsg_record__	*msgrec;		/* An address of the message records */
		const char		*facname;		/* A facility name, "HELP" - see __util$putmsg_fao() */
} EMSG_RECORD_DESC;


unsigned	__util$inimsg	(EMSG_RECORD_DESC *msgdsc);
unsigned	__util$getmsg	(unsigned sts, EMSG_RECORD **outmsg);
unsigned	__util$putmsg	(unsigned sts, ...);
unsigned	__util$putmsgd	(unsigned sts, const char *__mod, const char *__fi, unsigned __li, ...);
unsigned	__util$putmsg_fao	(unsigned sts, int prmcnt, ...);
unsigned	__util$putmsgd_fao	(unsigned sts, const char *__mod, const char *__fi, unsigned __li, int prmcnt, ...);


unsigned	__util$log	(const char *fac, unsigned severity, const char *fmt, ...);
unsigned	__util$logd	(const char *fac, unsigned severity, const char *fmt, const char *mod, const char *__func, unsigned __line, ...);
unsigned	__util$log_fao	(const char *a_fac, unsigned a_sev, const char *a_ctl, int a_prmcnt, ...);
unsigned	__util$logd_fao	(const char *a_fac, unsigned a_sev, const char *a_ctl, const char *a_mod, const char *a_func, unsigned a_line, int a_prmcnt, ...);
unsigned	__util$log2buf	(void *out, int outsz, int * outlen, const char *fac, unsigned severity, const char *fmt, ...);
unsigned	__util$syslog	(int fac, int sev, const char *tag, const char *msg, int  msglen);
unsigned	__util$out	(char *fmt, ...);


/*
**  $PUTMSG_FAO()/$LOG_FAO() cast each parameter to fao_prm_t themselves (the parameters travel through
**  varargs and are read as 64-bit values) and count them with FAO$_NARG(): write them plain.
*/
#define	FAO$_C(x)	, ((fao_prm_t)(x))
#define	FAO$_E0()
#define	FAO$_E1(a)	FAO$_C(a)
#define	FAO$_E2(a,b)	FAO$_C(a) FAO$_C(b)
#define	FAO$_E3(a,b,c)	FAO$_C(a) FAO$_C(b) FAO$_C(c)
#define	FAO$_E4(a,b,c,d)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d)
#define	FAO$_E5(a,b,c,d,e)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e)
#define	FAO$_E6(a,b,c,d,e,f)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f)
#define	FAO$_E7(a,b,c,d,e,f,g)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g)
#define	FAO$_E8(a,b,c,d,e,f,g,h)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h)
#define	FAO$_E9(a,b,c,d,e,f,g,h,i)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h) FAO$_C(i)
#define	FAO$_E10(a,b,c,d,e,f,g,h,i,j)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h) FAO$_C(i) FAO$_C(j)
#define	FAO$_E11(a,b,c,d,e,f,g,h,i,j,k)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h) FAO$_C(i) FAO$_C(j) FAO$_C(k)
#define	FAO$_E12(a,b,c,d,e,f,g,h,i,j,k,l)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h) FAO$_C(i) FAO$_C(j) FAO$_C(k) FAO$_C(l)
#define	FAO$_E13(a,b,c,d,e,f,g,h,i,j,k,l,m)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h) FAO$_C(i) FAO$_C(j) FAO$_C(k) FAO$_C(l) FAO$_C(m)
#define	FAO$_E14(a,b,c,d,e,f,g,h,i,j,k,l,m,n)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h) FAO$_C(i) FAO$_C(j) FAO$_C(k) FAO$_C(l) FAO$_C(m) FAO$_C(n)
#define	FAO$_E15(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h) FAO$_C(i) FAO$_C(j) FAO$_C(k) FAO$_C(l) FAO$_C(m) FAO$_C(n) FAO$_C(o)
#define	FAO$_E16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	FAO$_C(a) FAO$_C(b) FAO$_C(c) FAO$_C(d) FAO$_C(e) FAO$_C(f) FAO$_C(g) FAO$_C(h) FAO$_C(i) FAO$_C(j) FAO$_C(k) FAO$_C(l) FAO$_C(m) FAO$_C(n) FAO$_C(o) FAO$_C(p)
#define	FAO$_GET(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,NAME,...)	NAME

/* FAO$_NARG(): the number of parameters written, 0 to 16 */
#define	FAO$_NARG(...)	FAO$_GET(_0, ##__VA_ARGS__, 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define	FAO$_EACH(...)	FAO$_GET(_0, ##__VA_ARGS__, FAO$_E16,FAO$_E15,FAO$_E14,FAO$_E13,FAO$_E12,FAO$_E11,FAO$_E10,FAO$_E9,FAO$_E8,FAO$_E7,FAO$_E6,FAO$_E5,FAO$_E4,FAO$_E3,FAO$_E2,FAO$_E1,FAO$_E0)(__VA_ARGS__)

#ifndef	NDEBUG
	#define $PUTMSG(sts, ...)		__util$putmsgd(sts, __MODULE__, __FUNCTION__ , __LINE__ , ## __VA_ARGS__)
	#define $PUTMSG_FAO(sts, ...)		__util$putmsgd_fao(sts, __MODULE__, __FUNCTION__ , __LINE__ , FAO$_NARG(__VA_ARGS__) FAO$_EACH(__VA_ARGS__))
	#define $LOG(severity, fmt, ...)	__util$logd(__FAC__, severity, fmt, __MODULE__, __FUNCTION__ , __LINE__ , ## __VA_ARGS__)
	#define $LOG_FAO(severity, ctl, ...)	__util$logd_fao(__FAC__, severity, ctl, __MODULE__, __FUNCTION__ , __LINE__ , FAO$_NARG(__VA_ARGS__) FAO$_EACH(__VA_ARGS__))
	#define $IFLOG(f, severity, fmt, ...)	((f) ? __util$logd(__FAC__, severity, fmt, __MODULE__, __FUNCTION__ , __LINE__ , ## __VA_ARGS__) : (severity))
#else
	#define $PUTMSG(sts, ...)		__util$putmsg(sts, ## __VA_ARGS__)
	#define $PUTMSG_FAO(sts, ...)		__util$putmsg_fao(sts, FAO$_NARG(__VA_ARGS__) FAO$_EACH(__VA_ARGS__))
	#define $LOG(severity, fmt, ...)	__util$log(__FAC__, severity, fmt, ## __VA_ARGS__)
	#define $LOG_FAO(severity, ctl, ...)	__util$log_fao(__FAC__, severity, ctl, FAO$_NARG(__VA_ARGS__) FAO$_EACH(__VA_ARGS__))
	#define $IFLOG(f, severity, fmt, ...)	((f) ? __util$logd(__FAC__, severity, fmt, __MODULE__, __FUNCTION__ , __LINE__ , ## __VA_ARGS__) : (severity))

#endif



/*
 * Description: Implement spinlock logic by using GCC builtin, set the lock value to 1.
 *
 * Input:
 *	lock:	a pointer to longword, to accept a lock flag
 *
 * Return:
 *	STS$K_SUCCESS
 *	STS$K_ERROR
 */

#define	$LOCK_LONG(lock)	__util$lockspin(lock)

inline	static int __util$lockspin (void volatile * lock)
{
unsigned i = 0xffffffffU;


#ifdef	WIN32
int	status = 0;

	for ( ; i && !(status = TryAcquireSRWLockExclusive((SRWLOCK *) lock)); i--);

	return	status ? STS$K_SUCCESS : STS$K_ERROR;
#else
	for ( ; i && __sync_lock_test_and_set(((int *) lock), 1); i--)
		for (; i && (* ((int *)lock)); i--);

	return	i ? STS$K_SUCCESS : STS$K_ERROR;
#endif
}


/*
 * Description: Release has been set lock flag, set the lock value to 0.
 *
 * Input:
 *	lock:	a pointer to longword, to accept a lock flag
 *
 * Return:
 *	STS$K_SUCCESS
 */
#define	$UNLOCK_LONG(lock)	__util$unlockspin(lock)

inline	static	int __util$unlockspin (void * lock)
{

#if _WIN32
	ReleaseSRWLockExclusive( (SRWLOCK *) lock);
#else
	__sync_lock_release((int *)  lock);
#endif

	return	STS$K_SUCCESS;
}



/*
 * Condition codes of the queue routines and of __util$inimsg().  They follow the $DEFMSG/$DEFSEV layout: the severity is
 * in bits 0..2 (here ERROR, so the low bit is clear), the message number above it.
 */
#define	UTIL$S_INVARG	0x102		/* A NULL or otherwise unusable argument			*/
#define	UTIL$S_INQUE	0x202		/* The entry is on another queue already			*/
#define	UTIL$S_NOLOCK	0x302		/* The spinlock of the queue could not be acquired		*/
#define	UTIL$S_DUPFAC	0x400		/* __util$inimsg(): a catalogue of this facility is registered already (a warning)	*/

/*
 * Description: Take an entry out of the chain of the queue it stands in and re-point the head and the
 *	tail of the queue when the entry was the first or the last one.  The caller holds the spinlock;
 *	the counter and the entry's own queue link are left to the caller.
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *	ent:	The ENTRY to be unlinked
 *
 * Return:
 *	NONE
 */
inline static void __util$q_unlink (__QUEUE * que, ENTRY * ent)
{
	if ( ent->left )
		ent->left->right = ent->right;
	else	que->head = ent->right;

	if ( ent->right )
		ent->right->left = ent->left;
	else	que->tail = ent->left;

	ent->left = ent->right = NULL;
}


/*
 * Description: Remove all entries from the given queue
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *
 * Output:
 *	count:	A count of entries in the __QUEUE before cleanuping, 0 for an empty queue
 *
 * Return:
 *	condition code: STS$K_SUCCESS (also for an empty queue), STS$K_ERROR for a NULL argument
 *	or when the spinlock could not be acquired
 */
inline static int __util$clrqueue (void * que,  unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_ent, * _next;

	/*
	 * Sanity check
	 */
	if ( !_que || !count )
		return	STS$K_ERROR;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	/* Store entries counter, an empty queue reports zero */
	*count = _que->count;

	/* Run over all entries, zeroing the links and the reference to the queue ... */
	for ( _ent = _que->head; _ent; _ent = _next)
		{
		_next = _ent->right;
		_ent->left = _ent->right = NULL;
		_ent->queue = NULL;
		}

	_que->count = 0;
	_que->head = _que->tail = NULL;

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}


/*
 * Description: Insert a new entry at tail of the queue
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *	ent:	New ENTRY pointer
 *
 * Output:
 *	count:	A count of entries in the __QUEUE before inserting
 *
 * Return:
 *	STS$K_SUCCESS	- inserted, or the entry is on this queue already (nothing is changed)
 *	UTIL$S_INVARG	- a NULL argument
 *	UTIL$S_INQUE	- the entry is on another queue
 *	UTIL$S_NOLOCK	- the spinlock of the queue could not be acquired
 */
inline static int __util$insqtail (void * que, void * ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_entold, *_entnew = (ENTRY *) ent;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent || !count )
		return	UTIL$S_INVARG;

	/* Check that ENTRY has not been in the a __QUEUE already */
	if ( _entnew->queue == que )
		return	STS$K_SUCCESS;	/* Already: in the __QUEUE*/
	else if ( _entnew->queue )	/*    in other __QUEUE	*/
		return	UTIL$S_INQUE;


	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	UTIL$S_NOLOCK;

	*count = _que->count;

	if ( (_entold = _que->tail) )
		_entold->right	= _entnew;
	else	_que->head	= _entnew;

	_entnew->left	= _entold;
	_entnew->right	= NULL;
	_entnew->queue	= que;
	_que->tail	= _entnew;

	_que->count++;

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}

/*
 * Description: Insert a new entry at head of the queue
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *	ent:	New ENTRY pointer
 *
 * Output:
 *	count:	A count of entries in the __QUEUE before inserting
 *
 * Return:
 *	the same condition codes as __util$insqtail() : STS$K_SUCCESS, UTIL$S_INVARG, UTIL$S_INQUE,
 *	UTIL$S_NOLOCK
 */
inline static int __util$insqhead (void * que, void * ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_entold, *_entnew = (ENTRY *) ent;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent || !count )
		return	UTIL$S_INVARG;

	/* Check that ENTRY has not been in the a __QUEUE already */
	if ( _entnew->queue == que )
		return	STS$K_SUCCESS;	/* Already: in the __QUEUE*/
	else if ( _entnew->queue )	/*    in other __QUEUE	*/
		return	UTIL$S_INQUE;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	UTIL$S_NOLOCK;

	*count = _que->count;

	if ( (_entold = _que->head) )
		_entold->left	= _entnew;
	else	_que->tail = _entnew;

	_entnew->right	= _entold;
	_entnew->left	= NULL;
	_entnew->queue	= que;
	_que->head = _entnew;

	_que->count++;

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}


/*
 * Description: Remove the given entry from the queue it stands in.  An entry which is not on this
 *	queue - an empty queue included - is an error, since there is nothing to remove; unlike the
 *	removal at head/tail, where an empty queue is a normal outcome.
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *	ent:	The entry to be removed
 *
 * Output:
 *	count:	A count of entries in the __QUEUE before removing
 *
 * Return:
 *	STS$K_SUCCESS	- removed
 *	STS$K_ERROR	- a NULL argument, the entry is not on this queue, or the spinlock could not be
 *			  acquired
 */
inline static int __util$remqent (void * que, void *ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_ent = (ENTRY *) ent;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent || !count )
		return	STS$K_ERROR;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	*count = _que->count;

	/* Check that ENTRY is belong to the __QUEUE	*/
	if ( (_ent->queue != que) || !_que->count )
		{
		__util$unlockspin(&_que->lock);
		return	STS$K_ERROR;
		}

	/*
	**  Either neighbour may be absent: the entry being removed can stand at the head of the queue,
	**  at its tail, or be the only one there. Where a neighbour is absent it is the queue itself
	**  that has to be re-pointed.
	*/
	__util$q_unlink(_que, _ent);
	_ent->queue = NULL;

	/*
	 * Reset head & tail if queue is empty
	 */
	if ( !(--_que->count) )
		_que->head = _que->tail = NULL;

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}


/*
 * Description: Remove an entry from the tail of the queue.  An empty queue is not an error: the
 *	routine returns STS$K_SUCCESS with *ent set to NULL and *count set to zero - the caller tells
 *	"got an entry" from "nothing there" by *ent (or *count), the way every caller of these macros
 *	does.  This is the rule of __util$remqhead() as well.
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *
 * Output:
 *	ent:	A removed entry, NULL when the queue was empty
 *	count:	A count of entries in the __QUEUE before removing, 0 when it was empty
 *
 * Return:
 *	STS$K_SUCCESS	- done (see above)
 *	STS$K_ERROR	- a NULL argument, or the spinlock could not be acquired (*ent is untouched)
 */
inline static int __util$remqtail (void * que, void **ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_ent;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent || !count )
		return	STS$K_ERROR;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	*count = _que->count;

	if ( (_ent = _que->tail) )
		{
		__util$q_unlink(_que, _ent);
		_ent->queue = NULL;

		/*
		 * Reset head & tail if queue is empty
		 */
		if ( !(--_que->count) )
			_que->head = _que->tail = NULL;
		}

	*ent = _ent;

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}

/*
 * Description: Remove an entry from the head of the queue.  The empty-queue rule is that of
 *	__util$remqtail() : STS$K_SUCCESS, *ent = NULL, *count = 0.
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *
 * Output:
 *	ent:	A removed entry, NULL when the queue was empty
 *	count:	A count of entries in the __QUEUE before removing, 0 when it was empty
 *
 * Return:
 *	STS$K_SUCCESS	- done (see above)
 *	STS$K_ERROR	- a NULL argument, or the spinlock could not be acquired (*ent is untouched)
 */
inline static int __util$remqhead (void * que, void **ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_ent;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent || !count )
		return	STS$K_ERROR;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	*count = _que->count;

	if ( (_ent = _que->head) )
		{
		__util$q_unlink(_que, _ent);
		_ent->queue = NULL;

		/*
		 * Reset head & tail if queue is empty
		 */
		if ( !(--_que->count) )
			_que->head = _que->tail = NULL;
		}

	*ent = _ent;

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}

/*
 * Description: Move an existent entry to head of the queue.  Moving the entry which is the head
 *	already is a success and changes nothing.
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *	ent:	A pointer to ENTRY to be moved at head of queue (the entry itself, as $MOVQHEAD passes it)
 *
 * Output:
 *	NONE
 *
 * Return:
 *	STS$K_SUCCESS	- done
 *	STS$K_ERROR	- a NULL argument, the entry is not on this queue, or the spinlock could not be
 *			  acquired
 */
inline static int __util$movqhead (void * que, void * ent)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_ent = (ENTRY *) ent;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent )
		return	STS$K_ERROR;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	/*
	 * Is the entry with a given address live in the queue?
	 */
	if ( (_ent->queue != que) || !_que->head )
		{
		__util$unlockspin(&_que->lock);
		return	STS$K_ERROR;
		}

	/*
	 * Exclude the entry from the chain (this re-points the tail if it was the last one),
	 * put it in front of the old head.
	 */
	if ( _que->head != _ent )
		{
		__util$q_unlink(_que, _ent);

		_ent->right = _que->head;
		_que->head->left = _ent;
		_que->head = _ent;
		}

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}

/*
 * Description: Move an existent entry to end of queue.  Moving the entry which is the tail
 *	already is a success and changes nothing.
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *	ent:	A pointer to ENTRY to be moved at tail of queue (the entry itself, as $MOVQTAIL passes it)
 *
 * Output:
 *	NONE
 *
 * Return:
 *	STS$K_SUCCESS	- done
 *	STS$K_ERROR	- a NULL argument, the entry is not on this queue, or the spinlock could not be
 *			  acquired
 */
inline static int __util$movqtail (void * que, void * ent)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_ent = (ENTRY *) ent;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent )
		return	STS$K_ERROR;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	/*
	 * Is the entry with a given address live in the queue?
	 */
	if ( (_ent->queue != que) || !_que->tail )
		{
		__util$unlockspin(&_que->lock);
		return	STS$K_ERROR;
		}

	/*
	 * Exclude the entry from the chain (this re-points the head if it was the first one),
	 * put it behind the old tail.
	 */
	if ( _que->tail != _ent )
		{
		__util$q_unlink(_que, _ent);

		_ent->left = _que->tail;
		_que->tail->right = _ent;
		_que->tail = _ent;
		}

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}

/*
 * Macros over the VMS-ish double linked queue (__QUEUE of ENTRY), every modification interlocked by a GCC spinlock.
 *
 */

/*	Insert a new entry into the queue at head, return condition status, count - a number of
 *	entries in the queue before addition of the new element
 */
#define	$INSQHEAD(que, ent, count)	__util$insqhead ((__QUEUE *) que, (void *) ent, (unsigned *) count)

/*	Insert a new entry into the queue at tail, return condition status, count - a number of
 *	entries in the queue before addition of the new element
 */
#define	$INSQTAIL(que, ent, count)	__util$insqtail ((__QUEUE *) que, (void *) ent, (unsigned *) count)

/*	Get/Remove an entry from head of the queue, return condition status, count - a number of
 *	entries in the queue before removing of the element
 */
#define	$REMQHEAD(que, ent, count)	__util$remqhead ((__QUEUE *) que, (void **) ent, (unsigned *) count)

/*	Get/Remove an entry from tail of the queue, return condition status, count - a number of
 *	entries in the queue before removing of the element
 */
#define	$REMQTAIL(que, ent, count)	__util$remqtail ((__QUEUE *) que, (void **)ent, (unsigned *) count)


/*	Move an entry with a given address from current position of the queue to head of the queue,
 *	return condition status;
 */
#define	$MOVQHEAD(que, ent)		__util$movqhead ((__QUEUE *) que, (void *) ent)

/*	Move an entry with a given address from current position of the queue to tail of the queue,
 *	return condition status;
 */
#define	$MOVQTAIL(que, ent)		__util$movqtail ((__QUEUE *) que, (void *) ent)

/*	Remove a givent entry from the queueue.
 */
#define	$REMQENT(que, ent, count)	__util$remqent ((__QUEUE *) (que), (void *) (ent), (unsigned *) (count))


/*	Remove all entries form the queue.
 */
#define	$CLRQUE(que, count)	__util$clrqueue ((__QUEUE *) que, (unsigned *) count)


/* Macros to return minimal/maximum value from two given integers		*/
inline static int __util$min (int x, int y)
{
	return	(x < y) ? x : y;
}
#define	$MIN(x, y)	__util$min((int) (x), (int) (y))

inline static int __util$max (int x, int y)
{
	return	(x > y) ? x : y;
}
#define	$MAX(x, y)	__util$max((int) (x), (int) (y))

/* Return a result of checking value in the given range, 0/false - out of range, 1/true - is in range */
inline static int __util$isinrange (int x, int __min__, int __max__ )
{
int	left = $MIN(__min__, __max__) , right = $MAX(__min__, __max__);

	return ( (x < left) || ( x > right ) ) ? 0 : 1;
}

/* Return result of checking value between two borders: min and max	*/
#define	$ISINRANGE(x, __min__, __max__)	__util$isinrange(x, (__min__), (__max__))


/* Return a result of checking value in the given range, the borders [min; max] included (in any
 * order of the borders), 0/false - out of range, 1/true - is in range
 */
inline static int __util$isinrange2 (int x, int __min__, int __max__ )
{
int	left = $MIN(__min__, __max__) , right = $MAX(__min__, __max__);

	return ( (x < left) || ( x > right ) ) ? 0 : 1;
}

/* Return result of checking value between two borders: min and max	*/
#define	$ISINRANGE2(x, __min__, __max__)	__util$isinrange2(x, (__min__), (__max__))


/* Return value bound to left or right border eg. x = [min; max]	*/
inline static int __util$range (int x, int	__min__, int __max__ )
{
int	left = $MIN(__min__, __max__) , right = $MAX(__min__, __max__);

	x = $MAX(x, left);
	x = $MIN(x, right);

	return	x;
}

/* Return value bound to left or right border eg. x = [min; max]	*/
#define	$RANGE(x, __min__, __max__)	__util$range(x, __min__, __max__)



/*
 *  DESCRIPTION: __util$iszero	- check buffer for consecutive zero octets
 *
 *  INPUT:
 *	bufp:	buffer to check, address
 *	bufsz:	a length of data in the buffer, octets; there is nothing to see in a NULL buffer or
 *		a length of zero - it is "all zero"
 *
 *  RETURN:
 *	STS$K_SUCCESS	- the buffer is zero filled
 *	STS$K_WARN	- the buffer is not zero filled
 */
inline static int __util$iszero	(
		void *	bufp,
		int	bufsz
			)
{
const unsigned char *__bufp = (const unsigned char *) bufp;
unsigned long long	l_word;

	if ( !bufp )
		return	STS$K_SUCCESS;

	/* Eight octets a time; memcpy() keeps the access aligned and does not depend on the width of long */
	for ( ; bufsz >= (int) sizeof(l_word); bufsz -= (int) sizeof(l_word), __bufp += sizeof(l_word) )
		{
		memcpy(&l_word, __bufp, sizeof(l_word));

		if ( l_word )
			return	STS$K_WARN;
		}

	/* The rest, 0...7 octets */
	for ( ; bufsz > 0; bufsz--, __bufp++ )
		if ( *__bufp )
			return	STS$K_WARN;

	return	STS$K_SUCCESS;
}


/*
 *  DESCRIPTION: __util$bzero	- zeroing memory block
 *
 *  INPUT:
 *	bufp:	buffer to clear, address
 *	bufsz:	a length of the buffer, octets - every one of them is cleared
 *
 *  RETURN:
 *	STS$K_SUCCESS, or STS$K_ERROR for a NULL buffer of a non-zero length
 */
inline static int __util$bzero	(
		void *	bufp,
		size_t	bufsz
			)
{
	if ( !bufsz )
		return	STS$K_SUCCESS;

	if ( !bufp )
		return	STS$K_ERROR;

	memset(bufp, 0, bufsz);

	return	STS$K_SUCCESS;
}




/* Tracing facility	*/
void	__util$trace	(int cond, const char *fmt, const char *mod, const char *func, unsigned line, ...);

#ifndef	NDEBUG
	#ifndef	$TRACE
		#define $TRACE(fmt, ...)	__util$trace(1, fmt, __MODULE__, __FUNCTION__, __LINE__ , ## __VA_ARGS__)
	#endif

	#ifndef	$IFTRACE
		#define $IFTRACE(cond, fmt, ...) __util$trace(cond, fmt, __MODULE__, __FUNCTION__, __LINE__ , ## __VA_ARGS__)
	#endif
#else
#ifndef __UTIL$_NOPE__
static inline void __util$nope (void) {}
#define	__UTIL$_NOPE__	1
#endif /* __UTIL$_NOPE__ */



	#define $TRACE(fmt, ...)		__util$nope()
	#define $IFTRACE(cond, fmt, ...)	((void) sizeof(cond), __util$nope())

#endif

/* Counted (length-byte-prefixed) ASCII string, see VMS's ASCIC */


#define	ASC$K_SZ	255

typedef	struct _asc
{
	unsigned char	len;
	char	sts[ASC$K_SZ];

	#define	__ASC_TYPE__	1
} ASC;


/* Initialize an ASCIC structure by given string	*/
#define	$ASCDECL(a,sts)	ASC a = {(unsigned char)sizeof(sts)-1,(sts)}
#define $ASCINI(a)	((unsigned char) sizeof(a))-1,(a)
#define $ASCLEN(a)	(((ASC *) a)->len)
#define $ASCPTR(a)	(((ASC *) a)->sts)
#define	$ASC(a)		(((ASC *) a)->len),(((ASC *) a)->sts)
#define	$ASCNIL		0,0




/*
 *   DESCRIPTION: Convert 32-x bits unsigned interegr ot the decimal text string.
 *
 *   INPUT:
 *	a_src:		Source value to be converted, unsigned int
 *	a_dst:		An address of the destination buffer
 *	a_dstsz:	A size of the destination buffer
 *
 *   OUTPUT:
 *	[a_retlen]:	A length of the result decimal string, optional
 */
inline	static int __util$uint32_2_dec
		(
	unsigned int	a_src,
		char	*a_dst,
		size_t	a_dstsz,
		size_t	*a_retlen
		)
{
size_t		l_retlen;
char		l_tmp[16], *l_p;						/* 10 digits is the max for uint32 - 16 is enough */

	assert(a_dstsz);								/* Check for non-zero buffer */

	/*
	 * Build the decimal digits right-to-left into a scratch buffer. A do/while is
	 * used so that the value 0 is rendered as the single character "0".
	 */
	l_retlen = 0;
	l_p = l_tmp + sizeof(l_tmp);

	do	{
		*(--l_p) = (char) ('0' + (a_src % 10));				/* Remainder of the division is the current digit */
		a_src /= 10;							/* <a_src> keeps the result of the division */
		l_retlen++;
		}
	while ( a_src );

	/* Truncate to the destination capacity, always keeping room for the terminating NUL */
	if ( l_retlen > (a_dstsz - 1) )
		l_retlen = a_dstsz - 1;

	memcpy(a_dst, l_p, l_retlen);						/* Copy the (possibly truncated) result to the caller's buffer */
	a_dst[l_retlen] = '\0';

	if ( a_retlen )								/* Do we need to return the length of the decimal string ? */
		*a_retlen = l_retlen;

	return	(int) l_retlen;
}


/*
 *  DESCRIPTION: Copy an ASCIIZ string to the ASCIC container.  The container holds ASC$K_SZ octets
 *	and the copy is NUL-terminated, so a longer text is cut at ASC$K_SZ - 1 octets.
 *
 *  RETURN: the length of the text stored
 */
inline static int	__util$str2asc
			(
	const char *	a_src,
		void *	a_dst
			)
{
ASC	*l_dst = (ASC *) a_dst;

	l_dst->len = (unsigned char) strnlen(a_src, ASC$K_SZ - 1);
	memcpy(l_dst->sts, a_src, l_dst->len);
	l_dst->sts[l_dst->len] = '\0';

	return	l_dst->len;
}


/* Copying ASCIIZ string to ASCIC container */
inline static int	__util$asc2str
			(
		ASC *	src,
		char *	dst

			)
{
	memcpy(dst, $ASCPTR(src), $ASCLEN(src) );
	*(dst + $ASCLEN(src)) = '\0';

	return	$ASCLEN(src);
}




/*
 *  DESCRIPTION: Retrieve a substring with a given index
 *
 *  INPUT:
 *	a_src:		The ASCIC string of separated elements
 *	a_separator:	The separator octet
 *	a_idx:		The index of the element, from 0
 *
 *  OUTPUT:
 *	a_dst:		The element (NUL-terminated when it leaves room); an empty one when a_idx is
 *			negative or the string has fewer elements
 *
 *  RETURN:
 *	the length of the element, 0 if there is no such element
 */
inline static int	__util$strelem
			(
	const ASC	*a_src,
		int	a_separator,
		int	a_idx,
		ASC 	*a_dst
			)
{
const char	*l_cp = a_src->sts, *l_end = a_src->sts + a_src->len, *l_sep;
int	l_i, l_len;

	a_dst->len = 0;
	a_dst->sts[0] = '\0';

	if ( a_idx < 0 )
		return	0;

	/* Step over <a_idx> elements, each of them ends with a separator */
	for ( l_i = 0; l_i < a_idx; l_i++ )
		{
		if ( !(l_sep = (const char *) memchr(l_cp, a_separator, (size_t) (l_end - l_cp))) )
			return	0;

		l_cp = l_sep + 1;
		}

	l_sep = (const char *) memchr(l_cp, a_separator, (size_t) (l_end - l_cp));
	l_len = (int) ((l_sep ? l_sep : l_end) - l_cp);

	memcpy(a_dst->sts, l_cp, (size_t) l_len);
	if ( l_len < ASC$K_SZ )
		a_dst->sts[l_len] = '\0';

	a_dst->len = (unsigned char) l_len;

	return	l_len;
}




/*
 *  DESCRIPTION: Comparing two ASCIC
 */
inline static int	__util$cmpasc
			(
		ASC *	s1,
		ASC *	s2
			)
{
int	status;

	if ( (status = ($ASCLEN(s1) - $ASCLEN(s2))) )
		return	status;

	return	memcmp($ASCPTR(s1), $ASCPTR(s2), $ASCLEN(s1) );
}



/*
 *  DESCRIPTION: Comparing two ASCIC
 */
inline static int	__util$cmpasc_blind
			(
		const ASC *	s1,
		const ASC *	s2
			)
{
int	status;

	if ( (status = ($ASCLEN(s1) - $ASCLEN(s2))) )
		return	status;
#ifdef WIN32
	return	_stricmp($ASCPTR(s1), $ASCPTR(s2));
#else
	return	strncasecmp($ASCPTR(s1), $ASCPTR(s2), $ASCLEN(s1) );
#endif // WIN32

}






/* Copy ASCIC */
inline static int	__util$cpyasc
			(
		const ASC *	a_src,
		const ASC *	a_dst
			)
{

	$ASCLEN(a_dst) =  $ASCLEN(a_src);

	memmove($ASCPTR(a_dst), $ASCPTR(a_src), $ASCLEN(a_dst)) ;

	return	$ASCLEN(a_dst);
}





#define	$DUMPHEX(s,l)	__util$dumphex(__FUNCTION__, __LINE__ , s, l)
void	__util$dumphex	(const char *__fi, unsigned __li, const void *src, unsigned short srclen);

/*
** @RRL: Perform an addittion of two times with overflow control and handling.
*/
inline static void __util$add_time(
		const struct timespec* time1,
		const struct timespec* time2,
		struct timespec* result_time
		)
{
time_t sec = time1->tv_sec + time2->tv_sec;
long nsec = time1->tv_nsec + time2->tv_nsec;

	sec += nsec / 1000000000L;
	nsec = nsec % 1000000000L;

	result_time->tv_sec = sec;
	result_time->tv_nsec = nsec;
}


/*
** @RRL: Perform a subsctration of two times: result = time1 - time2.  The result is normalised -
** tv_nsec is in [0; 999999999], a negative difference of the nanoseconds borrows a second.
*/

/*
struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
	temp.tv_sec = end.tv_sec-start.tv_sec-1;
	temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
	temp.tv_sec = end.tv_sec-start.tv_sec;
	temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}
*/

inline static void __util$sub_time(const struct timespec * time1, const struct timespec* time2, struct timespec* result_time)
{
struct timespec temp;

	temp.tv_sec = time1->tv_sec - time2->tv_sec;
	temp.tv_nsec = time1->tv_nsec - time2->tv_nsec;

	while ( temp.tv_nsec < 0 )
		{
		temp.tv_sec--;
		temp.tv_nsec += 1000000000L;
		}

	while ( temp.tv_nsec >= 1000000000L )
		{
		temp.tv_sec++;
		temp.tv_nsec -= 1000000000L;
		}

	*result_time = temp;
}



/*
 *   Compare two times; only the sign of the result is defined, it is the sign of (time1 - time2)
 *   and it cannot overflow: the seconds are compared, not subtracted.
 *
 *   RETURN
 *	0	- time1 == time2
 *	< 0	- time1 < time2 (time1 is earlier)
 *	> 0	- time1 > time2 (time1 is later)
 */
inline static int __util$cmp_time(const struct timespec * time1, const struct timespec* time2)
{
	if ( time1->tv_sec != time2->tv_sec )
		return	(time1->tv_sec < time2->tv_sec) ? -1 : 1;

	if ( time1->tv_nsec != time2->tv_nsec )
		return	(time1->tv_nsec < time2->tv_nsec) ? -1 : 1;

	return	0;
}

/*
 * INCrement , DECrement ...
 */
#define	$ATOMIC_INC(val)	__sync_fetch_and_add (&val, 1)
#define	$ATOMIC_DEC(val)	__sync_fetch_and_sub (&val, 1)

/*
 * Processing command line options stuff
 */
#define OPTS$K_INT	0		/* Integer parameter type: decimal, 0x.. or 0.., exactly <sz> octets stored.
					   Size 1, 2, 8: UNSIGNED, a minus sign is refused, range 0..2^n-1.
					   Size 4 and 0 (an int): SIGNED, a minus sign is taken, range int32.
					   __util$showparams() prints them the same way (1/2/8 unsigned, 4/0 signed)	*/
#define OPTS$K_STR	1		/* Parameter is string			*/
#define OPTS$K_OPT	2		/* Presence - is ON, missing - - OFF	*/
#define	OPTS$K_PORT	3		/* A TCP/IP port number, in HOST byte order (unsigned short);
					   convert with htons() to put it on the wire	*/
#define	OPTS$K_IP4	4		/* A TCP/IP IP adddres , NBO		*/
#define	OPTS$K_CONF	5		/* The option is a name of configuration file, read at once by
					   __util$getparams(); inside a configuration file it is
					   rejected (no nesting)		*/
#define OPTS$K_PWD	6		/* Parameter is password - don't show it*/


typedef struct _opts	{
		ASC	name;		/* Options name				*/
		void *	ptr;		/* A pointer to buffer which will accept an option's value	*/
		size_t	sz;		/* A size of target buffer		*/
		int	type;		/* Value type, see OPTS$K_* constants	*/
	} OPTS;

#define	OPTS_NULL { {0, 0}, NULL, 0, 0}

int	__util$getparams	(int, char *[], const OPTS *);
int	__util$readconfig	(const char *, OPTS *);
int	__util$showparams	(const OPTS *opts);

int	__util$deflog		(const char *, const char *);
int	__util$setlogfd	(int);
int	__util$rewindlogfile	(size_t);
int	__util$rotatelogfile	(size_t, unsigned);
/*
 *  DESCRIPTION: __util$pattern_match - match a string against a wildcard pattern.  In the pattern
 *	'*' is any run of octets (empty too), '%' and '?' are exactly one octet, everything else stands
 *	for itself; there is no escape.  The string has no wildcards: a '*' in it is an ordinary octet.
 *	Case-sensitive, the whole string must match; iterative, not recursive.
 *
 *  RETURN:
 *	1 - matches, 0 - does not (also for a NULL argument)
 */
int	__util$pattern_match	(char * a_str, char * a_pattern);

/*
 *  DESCRIPTION: __util$base64 / __util$unbase64 - base64 (RFC 4648) encoding and
 *	decoding into a bounded buffer.  The encoder pads with '=' and appends a
 *	NUL when there is room for it beyond the text; the decoder accepts padded
 *	and unpadded input and rejects anything outside the alphabet.
 *
 *  INPUT:
 *	src:	an address of the data to convert
 *	srclen:	a length of the data, octets
 *	dst:	an address of the output buffer
 *	dstsz:	a size of the output buffer, octets
 *
 *  OUTPUT:
 *	dstlen:	a length of the result, octets - set even when dstsz is too small
 *
 *  RETURN:
 *	STS$K_SUCCESS, or STS$K_ERROR on a bad argument, malformed input or a
 *	buffer too small.
 */
int	__util$base64		(void *src, int srclen, void *dst, int dstsz, int *dstlen);
int	__util$unbase64		(void *src, int srclen, void *dst, int dstsz, int *dstlen);

char *	__util$strstr		(char *s1, size_t s1len, char *s2, size_t s2len);

/*
 *  DESCRIPTION: __util$crc32c - CRC-32 of a memory region.  Despite the name this is the IEEE 802.3
 *	(zlib, Ethernet) CRC-32, reflected polynomial 0xEDB88320 - NOT the Castagnoli CRC-32C; the check
 *	value of "123456789" is 0xCBF43926.  The name is kept, since programs depend on it.
 *
 *  INPUT:
 *	crc:	0 to start, or the result of the previous call to continue
 *	buf:	an address of the data
 *	buflen:	a number of octets
 *
 *  RETURN:
 *	the 32-bit checksum (not a condition code)
 */
unsigned	__util$crc32c	(unsigned int crc, const void *buf, size_t buflen);

/*
 *  DESCRIPTION: __util$faohex - format up to 16 octets as one hex-and-ASCII dump line.
 *
 *  INPUT:
 *	src, srclen:	the octets (more than 16 are cut to 16)
 *	out, outsz:	the output buffer, at least 67 octets
 *
 *  OUTPUT:
 *	outlen:	the length written; optional, may be NULL
 *
 *  RETURN:
 *	the length written, 0 if the buffer is too small or an argument is NULL
 */
int	__util$faohex		(const void *src, unsigned short srclen, char *out, unsigned short outsz, unsigned short *outlen);

/*
 *  DESCRIPTION: __util$hex2bin - convert a hexadecimal string in binary representative.  The input is validated
 *	first: an odd length or a character that is not a hexadecimal digit is an error and NOTHING is written.
 *
 *  INPUT:
 *	a_srchex:	An address of source data buffer to convert from
 *	a_dstbin:	An address of the output buffer to accept converted octets
 *	a_srchexlen:	A length of the source data (even)
 *
 *  RETURN:
 *	A length of the data in the output buffer, a_srchexlen / 2; 0 for an empty text; -1 for an invalid input
 */
inline	static int __util$hex2bin
		(
	const void *	a_srchex,
		void *	a_dstbin,
	unsigned short  a_srchexlen
		)
{
const unsigned char	*__srchex = (const unsigned char *) a_srchex;
unsigned char	*__dstbin = (unsigned char *) a_dstbin;
int	i, l_n = a_srchexlen / 2;

	if ( a_srchexlen % 2 )
		return	-1;

	for ( i = 0; i < a_srchexlen; i++ )
		if ( !isxdigit(__srchex[i]) )
			return	-1;

	for ( i = 0; i < l_n; i++, __srchex += 2 )
		__dstbin[i] = (unsigned char) (((isdigit(__srchex[0]) ? __srchex[0] - '0' : tolower(__srchex[0]) - 'a' + 10) << 4)
			| (isdigit(__srchex[1]) ? __srchex[1] - '0' : tolower(__srchex[1]) - 'a' + 10));

	return	l_n;
}

/*
 *  DESCRIPTION: __util$bin2hex - convert a sequence of bytes from binary form
 *
 *  INPUT:
 *	a_srcbin:	An address of source data buffer to convert from
 *	a_dsthex:	An address of the output buffer to accept hex-string
 *	a_srcbinlen:	A length of the source data
 *
 *  RETURN:
 *	A length of the data in the output buffer
 */
inline	static int __util$bin2hex
		(
	const void *	a_srcbin,
		void *	a_dsthex,
	unsigned short  a_srcbinlen
		)
{
unsigned char    l = 0, h = 0, *__srcbin = (unsigned char *) a_srcbin, *__dsthex = (unsigned char *) a_dsthex; ;
int	retlen = a_srcbinlen * 2;

	__dsthex[retlen] = '\0';

	for( ; a_srcbinlen; a_srcbinlen--, __dsthex += 2, __srcbin++)
		{
		h       = (*__srcbin) >> 4;
		h	&= 0x0F;
		l       = (*__srcbin) & 0x0F;

		*__dsthex    = (h < 10) ? h + '0' : h + 'a' - 10;
		*(__dsthex+1)= (l < 10) ? l + '0' : l + 'a' - 10;
		}


	return	retlen;
}



/*
 *  DESCRIPTION: __util$bin2dec - convert a sequence of bytes from binary form
 *
 *  INPUT:
 *	srcbin:	An address of source data buffer to convert from
 *	dsthex:	An address of the output buffer to accept hex-string
 *	srcbinlen:	A length of the source data
 *
 *  RETURN:
 *	A length of the data in the output buffer
 */








#define	$BIN2HEX(s,d,l)	__util$bin2hex((char*) s, (char*) d, (unsigned short) l)

/*
 *
 *  Description: cuts a trailing comment off a string: the string ends at the first <marker> octet which
 *	is not inside a quoted part.  A part in double ("...") or single ('...') quotes is skipped as it
 *	stands; the other kind of quote inside it is an ordinary octet, there is no escape, and a quote
 *	left open runs to the end of the string.
 *  Input:
 *	src:	a pointer to the buffer with string to process
 *	srclen:	a length of the source buffer
 *	marker:	the octet which starts a comment
 *
 *  Output:
 *	src:	the marker is replaced with NUL
 *
 *  Return:
 *	a length of the string without the comment (0 if there is nothing but the comment)
 *
 */
inline static int	__util$uncomment	(
		char	*src,
		int	srclen,
		char	marker
			)
{
int	l_i;
char	l_quote = '\0';

	if ( !srclen || !src )
		return 0;

	for ( l_i = 0; l_i < srclen; l_i++ )
		{
		if ( l_quote )
			{
			if ( src[l_i] == l_quote )
				l_quote = '\0';

			continue;
			}

		if ( src[l_i] == marker )
			{
			src[l_i] = '\0';
			return	l_i;
			}

		if ( (src[l_i] == '"') || (src[l_i] == '\'') )
			l_quote = src[l_i];
		}

	return	srclen;
}

/*
 *
 *  Description: removes leading and trailing spaces or tabs (HT and VT, CR, LF) from string
 *  Input:
 *	src:	a pointer to the buffer wit string to process
 *	srclen:	a length of the source buffer
 *
 *  Return:
 *	a length of the actual data in the source buffer
 *
 */
inline static int	__util$trim	(
		char	*src,
		int	srclen
			)
{
int	count;
char *	cp;

	if ( !srclen || !src )
		return 0;

	/*
	 * Trim all space and tabs at begin of the string ...
	 */
	for (count = 0, cp = src; isspace (*cp) && (count < srclen); count++, cp++);

	if ( count )
		{
		memmove(src, cp, srclen -= count);
		*(src + srclen) = '\0';
		}

	if ( !srclen )
		return 0;

	/*
	 * Trim spaces at end of string ...
	 */
	for (count = 0, cp = src + srclen; isspace (*(--cp)) && (count < srclen); count++);

	if ( count )
		{
		* (cp + 1) = '\0';
		srclen -= count;
		}

	return	srclen;
}

/*
 *  Description: removes all spaces or tabs (HT and VT, CR, LF) from string; the result is
 *	NUL-terminated when something was removed, the buffer of <srclen> octets is never overrun
 *  Input:
 *	src:	a pointer to the buffer with string to process
 *	srclen:	a length of the source buffer
 *
 *  Return:
 *	a length of the actual data in the source buffer
 *
 */
inline static int	__util$collapse	(
		char	*src,
		int	srclen
			)
{
int	count, l_srclen = srclen;
char *	cp;

	if ( !srclen || !src )
		return 0;

	/*
	 * Remove all spaces or tabs
	 */
	for (count = 0, cp = src; srclen; src++, srclen--)
		if ( !isspace ((unsigned char) *src) )
			{
			*(cp++) = *src;
			count++;
			}

	/* The NUL goes where a removed octet has been; with nothing removed the buffer has no room for it */
	if ( count < l_srclen )
		*cp = '\0';

	return	count;
}

/*
 *  DESCRIPTION: __UTIL$STRCAT_RX - concatenates given list of strings into the single area.
 *
 *  INPUT:
 *	dst:	a pointer to area to accept concatenation of the strings
 *	dstsz:	destination area size
 *	...:	pairs <char *string, int length>, ended with a NULL pointer; a zero length is skipped
 *
 *  OUTPUT:
 *	dstlen:	a returned actual data length (without the NUL)
 *
 *  RETURN:
 *	STS$K_SUCCESS	- all the strings are in the area and the NUL after them, so the strings may fill
 *			  it up to <dstsz> - 1 octets exactly;
 *	STS$K_ERROR	- a string does not fit (those before it are kept, NUL-terminated) or a bad
 *			  argument; nothing is ever written at dst[dstsz]
 */
inline static	int	__util$strcat_rx	(
	void	*	dst,
	int	dstsz,
	int *	dstlen,
	...
	)
{
va_list ap;
char *	srcp, *dstp = (char *) dst;
int	srclen;

	if ( !dstlen )
		return	STS$K_ERROR;

	*dstlen = 0;

	if ( !dst || (dstsz < 1) )
		return	STS$K_ERROR;

	/* Initialize the argument list. */
	va_start (ap, dstlen);

	/* NULL is a "end-of-list" marker */
	while ( (srcp = va_arg(ap, char *)) )
		{
		/* Get source length, zero - skip to next */
		if ( !(srclen = va_arg(ap, int)) )
			continue;

		/* One octet is kept for the NUL */
		if ( (srclen < 0) || (srclen > (dstsz - 1 - *dstlen)) )
			break;

		memmove(dstp + *dstlen, srcp, srclen);
		*dstlen += srclen;
		}

	va_end(ap);

	dstp[*dstlen] = '\0';

	return	srcp ? STS$K_ERROR : STS$K_SUCCESS;
}


/*
 *  DESCRIPTION: Do a translation a given keyword string to binary representative against given Keywords table
 *
 *  INPUTS:
 *	src:	a pointer to the buffer with string to process
 *	srclen:	a length of the source buffer
 *	kbtl:	A keyword table
 *	ktblsz:	A size of the keywords table (in elements)
 *	flags:	A bitfield mask for string comparing/matching flag:
 *		UTIL$K_LOOKUP_NCASE	- compare without regard to case
 *		UTIL$K_LOOKUP_ABBREV	- the string may be an abbreviation (a leading part) of a keyword,
 *					  as DCL takes them: a keyword spelled out in full wins, else
 *					  the abbreviation must select one keyword (entries with the same
 *					  value - synonyms - count as one)
 *
 *  OUTPUTS:
 *	kwd:	A has been found keyword's entry
 *
 *
 *  RETURNS:
 *	STS$K_SUCCESS	- keywords has been found, result in the <kwd>
 *	STS$K_WARN	- ABBREV: an ambiguous abbreviation, <kwd> - the first keyword it matches
 *	STS$K_ERROR	- no such keyword (or an empty string)
 *
 */
#define	UTIL$K_LOOKUP_NCASE	(1 << 0)
#define	UTIL$K_LOOKUP_ABBREV	(1 << 1)

typedef	struct _kwdent
{
	ASC	kwd;									/* Keyword string, ASCIC */
	union	{									/* Assosiated value/address */
		int	val;
		void *	ptr;
		};
} KWDENT;

inline	static	int	__util$lookup_key	(
		const	char	*a_src,
			int	a_srclen,

		KWDENT *	a_ktbl,
			int	a_ktblsz,

		KWDENT	**	a_kwd,

			int	a_flags
			)
{
KWDENT	*l_ktbl, *l_found = NULL;
int	l_ambig = 0;

	if ( a_srclen <= 0 )
		return	STS$K_ERROR;

	for ( l_ktbl = a_ktbl; a_ktblsz; a_ktblsz--, l_ktbl++)
		{
		/* Longer than the keyword, or (exact) not the same length - not this one */
		if ( (a_srclen > l_ktbl->kwd.len) || (!(a_flags & UTIL$K_LOOKUP_ABBREV) && (a_srclen != l_ktbl->kwd.len)) )
			continue;

		if ( (a_flags & UTIL$K_LOOKUP_NCASE) ?
#ifdef	WIN32
			_strnicmp
#else
			strncasecmp
#endif
			(a_src, l_ktbl->kwd.sts, a_srclen) : memcmp(a_src, l_ktbl->kwd.sts, a_srclen) )
			continue;

		/* Spelled out in full - this one, whatever else the abbreviation matches */
		if ( a_srclen == l_ktbl->kwd.len )
			{
			*a_kwd = l_ktbl;
			return	STS$K_SUCCESS;
			}

		if ( !l_found )
			l_found = l_ktbl;
		else if ( (l_found->val != l_ktbl->val) || (l_found->ptr != l_ktbl->ptr) )
			l_ambig = 1;						/* Another keyword, not a synonym */
		}

	/* No matching ... */
	if ( !l_found )
		return	STS$K_ERROR;

	*a_kwd = l_found;

	return	l_ambig ? STS$K_WARN : STS$K_SUCCESS;
}



/*
 *  DESCRIPTION: __UTIL$UUID2STR - convert UUID from binary form to the human readable text string.
 *
 *  INPUT:
 *	uuid:	UUID to be converted
 *	buf:	a buffer to accept text string
 *	bufsz:	a size of the output buffer
 *
 *  RETURN:
 *	length of the data in the output buffer.
 */
inline static int	__util$uuid2str	(
		void *	uuid,
		char *	buf,
		int	bufsz
			)
{
unsigned char	tmp [256], *puuid = (unsigned char *) uuid;
int	len;

	len = snprintf( (char *) tmp, sizeof(tmp) - 1, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		puuid[0], puuid[1], puuid[2], puuid[3], puuid[4], puuid[5], puuid[6], puuid[7],
		puuid[8], puuid[9], puuid[10], puuid[11], puuid[12], puuid[13], puuid[14], puuid[15]);

	memcpy(buf, tmp, len = $MIN(bufsz, len));
	buf[len] = '\0';

	return	len;
}




/*
 *  DESCRIPTION: __UTIL$FILL - Fill destination buffer with a pattern data.
 *
 *  INPUT:
 *	dst:	destination buffer to be filled
 *	dstsz:	a size of the destination buffer
 *	src:	a buffer with the pattern data
 *	srcsz:	a size of the pattern data
 *
 *  RETURN:
 *	STS$K_SUCCESS	- filled, the pattern is repeated and the last copy is cut to fit
 *	STS$K_ERROR	- a NULL address, a negative <dstsz> or a pattern of zero (or negative) size
 */
static inline	int	__util$fill	(
		void	*	dst,
		int		dstsz,
	const void	*	src,
		int		srcsz
			)
{
int	fillen = 0;
char *	_dst = (char *) dst;

	/* An empty pattern would never fill anything */
	if ( !dst || !src || (srcsz <= 0) || (dstsz < 0) )
		return	STS$K_ERROR;

	while ( dstsz )
		{
		fillen = $MIN(srcsz, dstsz);
		memcpy(_dst, src, fillen);

		dstsz	-= fillen;
		_dst	+= fillen;


		}

	return	STS$K_SUCCESS;
}



/*
 *  DESCRIPTION: __UTIL$STRXOR - XORing octet source string with a given key , result is in destination buffer.
 *
 *  INPUT:
 *	key:	a buffer with the key
 *	keysz:	a length of the key
 *	src:	a buffer with the data to be XOR-ed
 *	srcsz:	a length of the source data
 *	dst:	a buffer to accept XOR-ed data
 *	dstsz:	a size of the output buffer
 *	ctx:	an internal context , can be NULL if it is not used
 *
 *  RETURN:
 *	condition code
 */
static inline	int	__util$strxor	(
		void	*	key,
		int		keysz,
		void	*	src,
		int		srcsz,
		void	*	dst,
		int		dstsz,
		int	*	ctx
			)
{
unsigned char* kp, * sp, *dp, * ep;
int	sz = (srcsz > dstsz) ? dstsz : srcsz;

	if (ctx && (*ctx > 0) && (*ctx > keysz))		/* Check that <ctx> is in key's area */
		return	STS$K_ERROR;

	kp = (ctx && *ctx > 0) ? ((unsigned char *) key + (*ctx)) : (unsigned char *)key;	/* Set key's pointer to initial position */
	ep = (unsigned char *) key + keysz;			/* Compute end of key address for future use */

	for ( sp = (unsigned char *)src, dp = (unsigned char *)dst; sz; sz--, dp++, sp++, kp++ )
		{
		if ( kp >= ep  )				/* Check and reset a pointer to byte in the key buffer */
			kp = (unsigned char *)key;

		(*dp) = (*sp) ^ (*kp);				/* XOR single octet */
		}


	if ( ctx )						/* Do we need to keep context between calls */
		*ctx = (int) (kp - ((unsigned char *) key));	/* Store a current key's position */

	return	STS$K_SUCCESS;
}




/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Copy a source string into a destination of another length and pad the destination.
**	min(srclen, dstlen) octets are copied; when the destination is longer, the rest of it is
**	filled with the fill octet.  A source longer than the destination is cut.  The copy is
**	done with memmove(), so the two areas may overlap.
**
**  FORMAL PARAMETERS:
**
**	srclen	Address of the length of the source, octets
**	source	Address of the source
**	fill	Address of the fill octet; NULL means a zero octet
**	dstlen	Address of the length of the destination, octets
**	dest	Address of the destination
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	STS$K_SUCCESS.
**--
*/
static inline int	__util$movc5	(
		unsigned short  *srclen,
		char            *source,
		unsigned char   *fill,
		unsigned short  *dstlen,
		char            *dest
			)
{
unsigned short	l_n = (*srclen < *dstlen) ? *srclen : *dstlen;

	memmove (dest, source, l_n);

	if ( *dstlen > l_n )
		memset (dest + l_n, fill ? *fill : 0, (size_t) (*dstlen - l_n));

	return	STS$K_SUCCESS;
}


#ifdef	__TRACE__
//	#define	$TRACEBACK(expr)	{if ( (expr) ) __util$traceback ( #expr, (expr) ); }
#endif




/* Follow stuff are for debug/development purpose only	*/

typedef	struct __sym_rec__ {
	unsigned long long	val;		/* binary value or mask		*/
	unsigned char		len;
			char	*sym;		/* ASCII counted string		*/
} SYM_REC;





/*
 *  DESCRIPTION: translate mask of bits to human readable ASCII string
 *
 *  INPUTS:
 *      mask:   64-bits unsigned long long
 *      tbl:    a table of bit = bit's name
 *      src:    a buffer to accept output
 *      srcsz:  a size of the buffer
 *
 *  OUTPUTS:
 *      NONE
 *
 *  RETURNS:
 *	length of actual data in the output buffer
 */
static inline int	__util$mask2sym	(
	unsigned long long	 mask,
	const	SYM_REC		*sym,
			 char	*out,
			int	 outsz
			)
{
int	outlen = 0;

	if ( outsz < 1 )					/* not even room for the NUL */
		return	0;

	for ( ; sym->val && sym->sym; sym++)
		{
		if ( !(mask & sym->val) )
			continue;

		/* The name, the '|' before it unless it is the first one, and the NUL after all */
		if ( outsz > (sym->len + (outlen ? 1 : 0)) )
			{
			if ( outlen )
				{
				*(out++) = '|';
				outlen++;
				outsz--;
				}

			memcpy(out, sym->sym, sym->len);
			out	+= sym->len;
			outlen	+= sym->len;
			outsz	-= sym->len;
			}
		else	break;
		}

	*out = '\0';

	return	outlen;
}



/*
 *  DESCRIPTION:    translate value to human readable ASCII string
 *
 *  INPUTS:
 *      dst:    value, 64-bits unsigned long long
 *      tbl:    a table of bit = bit's name
 *      src:    a buffer to accept output
 *      srcsz:  a size of the buffer
 *
 *  OUTPUTS:
 *      NONE
 *
 *  RETURNS:
 *      length of actual data in the output buffer
 */
static inline int	__util$val2sym	(
	unsigned long long	 val,
	const	SYM_REC		*sym,
			 char	*out,
			int	 outsz
			)
{
int	outlen = 0;

	if ( outsz < 1 )					/* not even room for the NUL */
		return	0;

	for ( ; sym->len && sym->sym; sym++)
		{
		if ( (val != sym->val) )
			continue;

		if ( outsz > sym->len )
			{
			memcpy(out, sym->sym, sym->len);
			out	+= sym->len;
			outlen	+= sym->len;

			break;
			}
		}

	*out = '\0';

	return	outlen;
}

#define	$SYM_REC_INI(s) {s, sizeof(#s)-1,  #s}
#define	$SYM_REC_EOL	{0, 0, NULL}


#ifndef	WIN32
// #pragma GCC diagnostic pop
#endif

#pragma	pack (pop)

#ifdef __cplusplus
}
#endif

#endif	/*	__util$UTILS__	*/
