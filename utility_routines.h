#ifdef _WIN32
	#pragma once
	#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif

/*
**  Abstract: An API declaration for the general purpose and utility routines and  stuff ...
**
**  Author: Ruslan R. Laishev
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
**	27-OCT-2017	RRL	Added __util$movc5 (stolen OpenVMS version of the lib$movc5) - copy source string to destionation
**				place, fill rest of output buffer with a given character.
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
*/

#if _WIN32
#define _CRT_SECURE_NO_WARNINGS	1

#include	<Windows.h>
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
#include	<pthread.h>
#include	<unistd.h>


#ifndef	WIN32
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored  "-Wparentheses"
	#pragma GCC diagnostic ignored  "-Wpointer-sign"
	#pragma GCC diagnostic ignored	"-Wunused-result"
#endif

#define	CRLFCRLF_LW	0x0a0d0a0d
#define	CRLFCRLF	"\r\n\r\n"
#define	CRLF		"\r\n"


#ifndef	likely
	#define likely(x)       __builtin_expect((x),1)
#endif

#ifndef	unlikely
	#define unlikely(x)     __builtin_expect((x),0)
#endif


/*
 * For bagent logging
*/
extern void (*p_cb_log_f)(const char * buf, unsigned int olen);




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


static inline pid_t	gettid(void)
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


/*
	Performs a conditional atomic compare and update operation
  involving one or two longwords in the same lock region.  The value
  pointed to by source is compared with the longword old_value.  If
  they are equal, the longword new_value is conditionally stored into
  the value pointed to by dest.

  The store will not complete if the compare yields unequal values or
  if there is an intervening store to the lock region involved.  To
  be in the same lock region, source and dest must point to aligned
  longwords in the same naturally aligned 16-byte region.

  The function returns 0 if the store does not complete, and 1 if the
  store does complete.

  Syntax:

       int __CMP_STORE_LONG (volatile void *source, int old_value,
       int new_value,
       volatile void *dest);

	These built-in functions perform an atomic compare and swap. That is,
  if the current value of *ptr is oldval, then write newval into *ptr.

	The “bool” version returns true if the comparison is successful and newval
  is written. The “val” version returns the contents of *ptr before the operation.
*/
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
inline static	int	____time(struct timespec *tv)
{

#if    1

	timespec_get(tv, TIME_UTC);
	tv->tv_nsec /=1024;
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

inline static int	____time(struct timespec *tv)
{
	clock_gettime(CLOCK_REALTIME, tv);

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

typedef	struct	__ile3	{
	unsigned short	code,	/* Item List Element code	*/
			len;	/* A size of the buffer		*/
		char *	ptr;	/* An address of buffer to accept data */
	unsigned short * retlen;/* An actual length of the data returned in the buffer */
} ILE3;


/* Macros to initialize Item List with a given values */
#define	$ILE2INI(code, len, ptr)		{code, len, (char *)ptr}
#define	$ILE3INI(ile, code, len, ptr, retlen)	{ile.code = code, ile.len = len, ile.ptr = (char *)ptr, ile.retlen = retlen}
#define	$ILENULL	{0}

/*
 *
 * Queue & Entry types & macros definitions
 *
 */
#pragma	pack (push)
#pragma	pack	(8)

/*
** Special type is supposed to be used as a part of complex types. Typical usage is:
** struct my_buffer {
**	ENTRY	links;	// A part to be used by $INSQ/$REMQ macroses
**
**	unsigned short	buflen,
**			bufsz;
**	char	*pbuf;
**}
*/
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


#define $STS_ERROR(code) 	( ((code) & STS$K_ERROR ) || ((code) & STS$K_ERROR) )

/*
 * +---------------------+--------------------------+-------------------+
 * |  Facility (16 bits) | Message Number (13 bits) | Severity (3) bits |
 * +---------------------+--------------------------+-------------------+
 */
#define	$FAC(code)		((code) >> 16)
#define	$MSG(code)		((code) >> 3)
#define	$SEV(code)		((code) & 0x7)


#define	$DEFFAC(code)		((code) << 16)
#define	$DEFMSG(code)		(0xFFff0000UL & ((code) << 3)))
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

} EMSG_RECORD;



typedef struct __emsg_record_desc__ {
		struct __emsg_record_desc__	*link;		/* A link to next Message Descriptor */
		unsigned		msgnr;			/* A number of messages in the msgs[] */
		unsigned		facno;			/* A facility number */
		struct __emsg_record__	*msgrec;		/* An address of the message records */
} EMSG_RECORD_DESC;


unsigned	__util$inimsg	(EMSG_RECORD_DESC *msgdsc);
unsigned	__util$getmsg	(unsigned sts, EMSG_RECORD **outmsg);
unsigned	__util$putmsg	(unsigned sts, ...);
unsigned	__util$putmsgd	(unsigned sts, const char *__mod, const char *__fi, unsigned __li, ...);


unsigned	__util$log	(const char *fac, unsigned severity, const char *fmt, ...);
unsigned	__util$logd	(const char *fac, unsigned severity, const char *fmt, const char *mod, const char *__func, unsigned __line, ...);
unsigned	__util$log2buf	(void *out, int outsz, int * outlen, const char *fac, unsigned severity, const char *fmt, ...);
unsigned	__util$syslog	(int fac, int sev, const char *tag, const char *msg, int  msglen);
unsigned	__util$out	(char *fmt, ...);

#ifndef	NDEBUG
	#define $PUTMSG(sts, ...)		__util$putmsgd(sts, __MODULE__, __FUNCTION__ , __LINE__ , ## __VA_ARGS__)
	#define $LOG(severity, fmt, ...)	__util$logd(__FAC__, severity, fmt, __MODULE__, __FUNCTION__ , __LINE__ , ## __VA_ARGS__)
	#define $IFLOG(f, severity, fmt, ...)	f ? __util$logd(__FAC__, severity, fmt, __MODULE__, __FUNCTION__ , __LINE__ , ## __VA_ARGS__) : severity
#else
	#define $PUTMSG(sts, ...)		__util$putmsg(sts, ## __VA_ARGS__)
	#define $LOG(severity, fmt, ...)	__util$log(__FAC__, severity, fmt, ## __VA_ARGS__)
	#define $IFLOG(f, severity, fmt, ...)	f ? __util$logd(__FAC__, severity, fmt, __MODULE__, __FUNCTION__ , __LINE__ , ## __VA_ARGS__)	: severity

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
 * Description: Remove all entries from the given queue
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *
 * Output:
 *	count:	A count of entries in the __QUEUE before cleanuping
 *
 * Return:
 *	condition code
 */
inline static int __util$clrqueue (void * que,  unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_ent;
int	nums;

	/*
	 * Sanity check
	 */
	if ( !_que || !count )
		return	STS$K_ERROR;

	if ( !_que->count )
		return	STS$K_SUCCESS;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	/* Store entries counter */
	*count = nums = _que->count;

	/* Run over all entries, zeroing link to queue ... */
	for ( _ent = _que->head; nums; nums--, _ent = _ent->right)
		_ent->queue = NULL;

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
 *	condition code
 */

#define	UTIL$S_INVARG	0x102
#define	UTIL$S_INQUE	0x202
#define	UTIL$S_NOLOCK	0x202

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

	if ( _entold = _que->tail )
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
 *	condition code
 */
inline static int __util$insqhead (void * que, void * ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_entold, *_entnew = (ENTRY *) ent;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent || !count )
		return	STS$K_ERROR;

	/* Check that ENTRY has not been in the a __QUEUE already */
	if ( _entnew->queue == que )
		return	STS$K_SUCCESS;	/* Already: in the __QUEUE*/
	else if ( _entnew->queue )	/*    in other __QUEUE	*/
		return	STS$K_ERROR;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	*count = _que->count;

	if ( _entold = _que->head )
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
 * Description: Remove an entry from the the current place in the queue
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *
 * Output:
 *	ent:	A removed entry
 *	count:	A count of entries in the __QUEUE before inserting
 *
 * Return:
 *	condition code
 */
inline static int __util$remqent (void * que, void *ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_ent = (ENTRY *) ent, *_entleft;

	/*
	 * Sanity check
	 */
	if ( !_que || !ent || !count )
		return	STS$K_ERROR;

	/* Check that ENTRY is belong to the __QUEUE	*/
	if ( _ent->queue != que )
		return	STS$K_ERROR;

	/*
	 * Acquire lock
	 */
	if ( !(1 & __util$lockspin( &_que->lock)) )
		return	STS$K_ERROR;

	if ( !(*count = _que->count) )
		{
		__util$unlockspin(&_que->lock);
		return STS$K_SUCCESS;
		}

	/*
	** Main work ...
	*/
	_entleft = _ent->left;
	_entleft->right = _ent->right;

	/*
	 * Reset head & tail if queue is empty
	 */
	if ( !(--_que->count) )
		_que->head = _que->tail = NULL;

	_ent->left = _ent->right = NULL;
	_ent->queue = NULL;

	/*
	 * Release the spinlock
	 */
	__util$unlockspin(&_que->lock);

	return	STS$K_SUCCESS;
}







/*
 * Description: Remove an entry from the tail of the queue
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *
 * Output:
 *	ent:	A removed entry
 *	count:	A count of entries in the __QUEUE before inserting
 *
 * Return:
 *	condition code
 */
inline static int __util$remqtail (void * que, void **ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_entleft, * _entright = NULL;

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

	if ( !(*count = _que->count) )
		{
		__util$unlockspin(&_que->lock);
		return STS$K_SUCCESS;
		}

	/*
	** Main work ...
	*/
	if ( _entright = _que->tail )
		{
		if ( _entleft = _entright->left )
			_entleft->right = NULL;

		_que->tail = _entleft;
		}

	*ent	= _entright;
	_entright->left = _entright->right = NULL;
	_entright->queue = NULL;

	/*
	 * Reset head & tail if queue is empty
	 */
	if ( !(--_que->count) )
		_que->head = _que->tail = NULL;

	/*
	 * Release the spinlock
	 */
	__util$unlockspin(&_que->lock);

#ifndef	WIN32
	assert(_entright);
#endif

	return	_entright ? STS$K_SUCCESS : STS$K_ERROR;
}

/*
 * Description: Remove an entry from the head of the queue
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *
 * Output:
 *	ent:	A removed entry
 *	count:	A count of entries in the __QUEUE before inserting
 *
 * Return:
 *	condition code
 */
inline static int __util$remqhead (void * que, void **ent, unsigned * count)
{
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	_entleft = NULL, * _entright;

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

	if ( !(*count = _que->count) )
		{
		__util$unlockspin(&_que->lock);
		return STS$K_SUCCESS;
		}

	/*
	** Main work ...
	*/
	if ( _entleft = _que->head )
		{
		if ( _entright = _entleft->right )
			_entright->left = NULL;

		_que->head = _entright;
		}
#ifndef	WIN32
	assert(_entleft);
#endif

	*ent	= _entleft;
	_entleft->left = _entleft->right = NULL;
	_entleft->queue = NULL;

	/*
	 * Reset head & tail if queue is empty
	 */
	if ( !(--_que->count) )
		_que->head = _que->tail = NULL;

	/*
	 * Release the spinlock
	 */
	__util$unlockspin(&_que->lock);

#ifndef	WIN32
	assert(_entleft);
#endif


	return	_entleft ? STS$K_SUCCESS : STS$K_ERROR;
}

/*
 * Description: Move an existen entry at head of queue.
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *	ent:	A pointer to ENTRY to be moved at head of queue
 *
 * Output:
 *	NONE
 *
 * Return:
 *	condition code
 */
inline static int __util$movqhead (void * que, void **ent)
{
unsigned	i;
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	entp, * _entleft = NULL, * _entright = NULL;

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

	if ( !(_que->count) )
		{
		__util$unlockspin(&_que->lock);
		return STS$K_SUCCESS;
		}

	/*
	 * Lookup in the queue an entry with a given address
	 */
	for (i = 0, entp = _que->head; i < _que->count; i++, entp = entp->right)
		{
		if ( entp == (ENTRY *) ent )
			break;
		}

	/*
	 * Is the entry already on head of the queue?
	 */
	if ( !i )
		return	__util$unlockspin(&_que->lock);

	/*
	 * Is the entry with a given address live in the queue?
	 */
	if ( !entp )
		{
		__util$unlockspin(&_que->lock);
		return	STS$K_ERROR;
		}

	/*
	 * Exclude has been found entry from the chain
	 */
	if ( _entleft = entp->left )
		_entleft->right = entp->right;
#ifndef	WIN32
	else	assert ( entp->left );	/* left link can be only on first entry! */
#endif


	if ( _entright= entp->right )
		_entright->left = entp->left;

	/*
	 * Put has been found entry at head of tail
	 */
	entp->right = _que->head;
	entp->left = NULL;
	_que->head = entp;

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}

/*
 * Description: Move an existen entry to end of queue.
 *
 * Input:
 *	que:	A pointer to __QUEUE structure
 *	ent:	A pointer to ENTRY to be moved at head of queue
 *
 * Output:
 *	NONE
 *
 * Return:
 *	condition code
 */
inline static int __util$movqtail (void * que, void **ent)
{
unsigned	i;
__QUEUE * _que = (__QUEUE *) que;
ENTRY *	entp, * _entleft = NULL, * _entright = NULL;

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

	if ( !(_que->count) )
		{
		__util$unlockspin(&_que->lock);
		return STS$K_SUCCESS;
		}

	/*
	 * Lookup in the queue an entry with a given address
	 */
	for (i = 0, entp = _que->head; i < _que->count; i++, entp = entp->right)
		{
		if ( entp == (ENTRY *) ent )
			break;
		}

	/*
	 * Is the entry already at tail ?
	 */
	if ( i == _que->count )
		return	__util$unlockspin(&_que->lock);

	/*
	 * Is the entry with a given address live in the queue?
	 */
	if ( !entp )
		{
		__util$unlockspin(&_que->lock);
		return	STS$K_ERROR;
		}

	/*
	 * Exclude has been found entry from the chain
	 */
	if ( _entleft = entp->left )
		_entleft->right = _entright;

	if ( _entright= entp->right )
		_entright->left = _entleft;
#ifndef	WIN32
	else assert( entp->right );	/* Right link can be on the last entry ! */
#endif

	/*
	 * Put has been found entry at tail of queue
	 */
	entp->left = _que->tail;
	entp->right = NULL;
	_que->tail = entp;

	/*
	 * Release the spinlock
	 */
	return	__util$unlockspin(&_que->lock);
}

/*
 * A set of macros to implement good old hardcore school ... of VMS-ish double linked lists - queue,
 * all queue modifications using interlocking by using GCC spinlocks.
 *
 * Typical scenario of using:
 *
 * __QUEUE	myqueue = QUEUE_INITIALIZER;
 * struct message {
 *	ENTRY		entry;	// A space reservation for stuff has been used my the queue's macros
 *	unsigned short	len;
 *	unsigned char	buf[128];
 * } messages [ 32 ];
 *
 * int	main	(void)
 * {
 * int status, count;
 *
 *	// The queue preinitialization, by inserting element from statical allocated array
 *	for (int i ; i < sizeof(messages)/sizeof(messages[0)]); i++)
 *		$INSQTAIL(&myqueue, &messages[i], &count);
 * }
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
#define	$REMQENT(que, ent, count)	__util$remqent ((__QUEUE *) que, (void **)ent, (unsigned *) count)


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


/* Return a result of checking value in the given range include border,
 * 0/false - out of range, 1/true - is in range
 */
inline static int __util$isinrange2 (int x, int __min__, int __max__ )
{
int	left = $MIN(__min__, __max__) , right = $MAX(__min__, __max__);

	return ( (x <= left) || ( x >= right ) ) ? 0 : 1;
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
 *
 *  Description: check buffer for consecutive zero octets
 *
 *  Input:
 *
 *	bufp:	buffer to check, address
 *	bufsz:	a length of data in the buffer
 *
 *
 *  Return:
 *	SS$_NORMAL	- the buffer is zero filled
 *	STS$K_WARN	- the buffer is not zero filled
 */
/**
 * @brief __util$iszero	- check buffer for consecutive zero octets
 * @param bufp	:	buffer to check, address
 * @param bufsz	:	a length of data in the buffer
 * @return -
 * *	SS$_NORMAL	- the buffer is zero filled
 *	STS$K_WARN	- the buffer is not zero filled
 */
inline static int __util$iszero	(
		void *	bufp,
		int	bufsz
			)
{
int	i;
unsigned char *__bufp = (unsigned char *) bufp;

#if ( (defined ULLONG_MAX && (ULLONG_MAX > UINT_MAX)) || (defined ULONG_LONG_MAX && (ULONG_LONG_MAX > UINT_MAX)) )

//#pragma message	("%CC-I-ULLONG64, unsigned long long - 64 bits")

	/* Step by 8 octets ... */
	for (i = bufsz/sizeof(unsigned long long); i; i--, __bufp += sizeof(unsigned long long) )
		{
		if ( *((unsigned long long *) __bufp ) )
			return	STS$K_WARN;
		}
#endif // (sizeof(unsigned long long)) > sizeof(unsigned int long) )

	/* Compute remainder octets in the buffer */
	bufsz %= sizeof(unsigned int long);

	/* Step by 4 octets ... */
	for (i = bufsz/sizeof(unsigned int); i; i--, __bufp += sizeof(unsigned int) )
		{
		if ( *((unsigned int *) __bufp ) )
			return	STS$K_WARN;
		}

	/* Compute remainder octets in the buffer */
	bufsz %= sizeof(unsigned int);

	/* Step by 2 octets ... */
	for (i = bufsz/sizeof(unsigned short); i; i--, __bufp += sizeof(unsigned short) )
		{
		if ( *((unsigned short *) __bufp ) )
			return	STS$K_WARN;
		}

	/* Compute remainder octets in the buffer */
	bufsz %= sizeof(unsigned short);

	if ( bufsz && *__bufp )
		return	STS$K_WARN;

	return	STS$K_SUCCESS;
}


/**
 * @brief __util$bzero	- zeroing memory block
 * @param bufp	:	buffer to check, address
 * @param bufsz	:	a length of data in the buffer
 * @return -
 * 	SS$_NORMAL	- the buffer is zero filled
 *
 */
inline static int __util$bzero	(
		void *	bufp,
		size_t	bufsz
			)
{
size_t	i;
unsigned char *__bufp = (unsigned char *) bufp;

	/*
	 * There is a place for optimization : align a memory address at head
	 * of the memory block
	 */

#if ( (defined ULLONG_MAX && (ULLONG_MAX > UINT_MAX)) || (defined ULONG_LONG_MAX && (ULONG_LONG_MAX > UINT_MAX)) )

//#pragma message	("%CC-I-ULLONG64, unsigned long long - 64 bits")

	/* Step by 8 octets ... */
	for (i = bufsz/sizeof(unsigned long long); i; i--, __bufp += sizeof(unsigned long long) )
		*((unsigned long long *) __bufp ) = 0;
#endif // (sizeof(unsigned long long)) > sizeof(unsigned int) )

	/* Compute remainder octets in the buffer */
	bufsz %= sizeof(unsigned long long);

	/* Step by 4 octets ... */
	for (i = bufsz/sizeof(unsigned int); i; i--, __bufp += sizeof(unsigned int) )
		*((unsigned int *) __bufp ) = 0;

	/* Compute remainder octets in the buffer */
	bufsz %= sizeof(unsigned int);

	/* Step by 2 octets ... */
	for (i = bufsz/sizeof(unsigned short); i; i--, __bufp += sizeof(unsigned short) )
		if ( *((unsigned short *) __bufp ) )

	/* Compute remainder octets in the buffer */
	bufsz %= sizeof(unsigned short);

	if ( bufsz )
		__bufp = (unsigned char *)'\0';

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
	#define $TRACE(fmt, ...)	{}
	#define $IFTRACE(cond, fmt, ...)	{}

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
#define $ASCINI(a)	(unsigned char)sizeof(a)-1,(a)
#define $ASCLEN(a)	(((ASC *) a)->len)
#define $ASCPTR(a)	(((ASC *) a)->sts)
#define	$ASC(a)		(((ASC *) a)->len),(((ASC *) a)->sts)
#define	$ASCNIL		0,0


/* Copying ASCIIZ string to ASCIC container */
inline static int	__util$str2asc
			(
		char *	src,
		ASC *	dst
			)
{
	$ASCLEN(dst) = (unsigned char) strnlen(src, ASC$K_SZ);
	memcpy($ASCPTR(dst), src, $ASCLEN(dst) );
	dst->sts[dst->len] = '\0';

	return	$ASCLEN(dst);
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




/* Comparing two ASCIC */
inline static int	__util$cmpasc
			(
		ASC *	s1,
		ASC *	s2
			)
{
int	status;

	if ( status = ($ASCLEN(s1) - $ASCLEN(s2)) )
		return	status;

	return	memcmp($ASCPTR(s1), $ASCPTR(s2), $ASCLEN(s1) );
}



/* Comparing two ASCIC */
inline static int	__util$cmpasc_blind
			(
		ASC *	s1,
		ASC *	s2
			)
{
int	status;

	if ( status = ($ASCLEN(s1) - $ASCLEN(s2)) )
		return	status;

	return	strncasecmp($ASCPTR(s1), $ASCPTR(s2), $ASCLEN(s1) );
}






/* Copy ASCIC */
inline static int	__util$cpyasc
			(
		ASC *	a_src,
		ASC *	a_dst
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
** @RRL: Perform a subsctration of two times.
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

inline static void __util$sub_time(struct timespec * time1, struct timespec* time2, struct timespec* result_time)
{
struct timespec temp;

	temp.tv_sec = time1->tv_sec - time2->tv_sec;
	temp.tv_nsec = time1->tv_nsec - time2->tv_nsec;

	*result_time = temp;
}



/*
 *   RETURN
 *	0	- time1 == time2
 *	0 >	- time1 < time2
 *	0 <	- time1 > time2
 */
inline static int __util$cmp_time(struct timespec * time1, struct timespec* time2)
{
	if ( time1->tv_sec - time2->tv_sec )
		return	(time1->tv_sec - time2->tv_sec);

	return	(time1->tv_nsec - time2->tv_nsec);
}

/*
 * INCrement , DECrement ...
 */
#define	$ATOMIC_INC(val)	__sync_fetch_and_add (&val, 1)
#define	$ATOMIC_DEC(val)	__sync_fetch_and_sub (&val, 1)

/*
 * Processing command line options stuff
 */
#define OPTS$K_INT	0		/* Integer parameter type		*/
#define OPTS$K_STR	1		/* Parameter is string			*/
#define OPTS$K_OPT	2		/* Presence - is ON, missing - - OFF	*/
#define	OPTS$K_PORT	3		/* A TCP/IP port number, NBO		*/
#define	OPTS$K_IP4	4		/* A TCP/IP IP adddres , NBO		*/
#define	OPTS$K_CONF	5		/* The options is a name of configuration
					file to be processed recursively	*/
#define OPTS$K_PWD	6		/* Parameter is password - don't show it*/


typedef struct _opts	{
		ASC	name;		/* Options name				*/
		void *	ptr;		/* A pointer to buffer which will accept an option's value	*/
		size_t	sz;		/* A size of target buffer		*/
		int	type;		/* Value type, see OPTS$K_* constants	*/
	} OPTS;

#define	OPTS_NULL { {0, 0}, NULL, 0, 0}

int	__util$getparams	(int, char *[], const OPTS *);
int	__util$readparams	(int, char *[], OPTS *);
int	__util$readconfig	(char *, OPTS *);
int	__util$showparams	(const OPTS *opts);

int	__util$deflog		(const char *, const char *);
int	__util$rewindlogfile	(ssize_t);
int	__util$pattern_match	(char * str$, char * pattern$);

char *	__util$strstr		(char *s1, size_t s1len, char *s2, size_t s2len);

unsigned	__util$crc32c	(unsigned int crc, const void *buf, size_t buflen);

/**
 * @brief __util$hex2bin - convert a hexdecimal string in binary representative.
 *		It's expected that output buffer have enough space to accept
 *		 <(source_length + 1) / 2> octets.
 *
 * @param srchex	-	An address of source data buffer to convert from
 * @param dstbin	-	An address of the output buffer to accept converted octets
 * @param srchexlen	-	A length of the source data
 *
 * @return	-	A length of the data in the output buffer
 *
 */
inline	static int __util$hex2bin
		(
		void *	srchex,
		void *	dstbin,
	unsigned short  srchexlen
		)
{
unsigned char	c, l = 0, h = 0, *__srchex = (unsigned char *) srchex, *__dstbin = (unsigned char *) dstbin;
int	retlen = (srchexlen + 1) / 2, i;

	/* We will converting from tail to head */
	__srchex += (srchexlen - 1);
	__dstbin += (retlen - 1);

	for( i = (srchexlen / 2); i; i--, __dstbin--, __srchex--)
		{
		c = tolower(*__srchex);
		l = ((c) <= '9') ? (c) - '0' : (c) - 'a' + 10;

		__srchex--;

		c = tolower(*__srchex);
		h = ((c) <= '9') ? (c) - '0' : (c) - 'a' + 10;


		*__dstbin    = c = l | (h << 4);
		}

	if ( srchexlen % 2)
		{
		c = tolower(*__srchex);
		l = ((c) <= '9') ? (c) - '0' : (c) - 'a' + 10;

		*__dstbin    = c = l;
		}



	return	retlen;
}

/**
 * @brief __util$bin2hex - convert a sequence of bytes from binary from
 *		to a hexadecimal string. It's expected that output buffer have
 *		enough space to accept <source_length * 2> characters.
 *
 * @param srcbin	-	An address of source data buffer to convert from
 * @param dsthex	-	An address of the output buffer to accept hex-string
 * @param srcbinlen	-	A length of the source data
 *
 * @return	-	A length of the data in the output buffer
 *
 */
inline	static int __util$bin2hex
		(
		void *	srcbin,
		void *	dsthex,
	unsigned short  srcbinlen
		)
{
unsigned char    l = 0, h = 0, *__srcbin = (unsigned char *) srcbin, *__dsthex = (unsigned char *) dsthex; ;
int	retlen = srcbinlen * 2;

	__dsthex[retlen] = '\0';

	for( ; srcbinlen; srcbinlen--, __dsthex += 2, __srcbin++)
		{
		h       = (*__srcbin) >> 4;
		h	&= 0x0F;
		l       = (*__srcbin) & 0x0F;

		*__dsthex    = (h < 10) ? h + '0' : h + 'a' - 10;
		*(__dsthex+1)= (l < 10) ? l + '0' : l + 'a' - 10;
		}


	return	retlen;
}

#define	$BIN2HEX(s,d,l)	__util$bin2hex((char*) s, (char*) d, (unsigned short) l)

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
inline static int	__util$uncomment	(
		char	*src,
		int	srclen,
		char	marker
			)
{
int	count;
char *	cp, *cmnt = NULL;

	if ( !srclen || !src )
		return 0;

	/*
	 * Remove comments from end of string
	 */
	for (count = srclen, cp = src + (srclen - 1); count; count--, cp--)
		cmnt = *cp == marker ? cp : cmnt;

	if ( cmnt )
		{
		*cmnt = '\0';
		srclen = (int) (cmnt - src);
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
 *
 *  Description: removes all spaces or tabs (HT and VT, CR, LF) from string
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
int	count;
char *	cp;

	if ( !srclen || !src )
		return 0;

	/*
	 * Remove all spaces or tabs
	 */
	for (count = 0, cp = src; srclen; src++, srclen--)
		if ( !isspace (*src) )
			{
			*(cp++) = *src;
			count++;
			}

	*cp = '\0';

	return	count;
}

/**
 * @brief __UTIL$STRCAT_RX - concatenates given list of strings into the single area.
 *
 * @param dst :	a pointer to area to accept concatenation of the strings
 * @param dstsz : destination area size
 * @param dstlen : a returned actual data length
 *
 * @return -	STS$K_SUCCESS
 *		STS$K_ERROR - output buffer is overflow
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
int	srclen, status = STS$K_ERROR;

	/* Initialize the argument list. */
	va_start (ap, dstlen);

	/* Looping until destination buffer have a free space */
	for ( *dstlen = srclen = 0; (dstsz - *dstlen); )
		{
		/* NULL is a "end-of-list" marker */
		if ( !(srcp = va_arg(ap, char *)) )
			{
			status = STS$K_SUCCESS;
			break;
			}

		/* Get source length, zero - skip to next */
		if ( !(srclen = va_arg(ap, int)) )
			continue;

		if ( srclen > (dstsz - *dstlen) )
			break;

		memmove(dstp + *dstlen, srcp, srclen);
		*dstlen += srclen;
		}

	*(dstp + *dstlen) = '\0';
	va_end(ap);

	return	status;
}


/*
*/
#define	util$K_LOOKUP_NCASE	(1 << 0)
#define	util$K_LOOKUP_ABBREV	(1 << 1)

typedef	struct _kwdent
{
	ASC *	kwd;
	union	{
		int	val;
		void *	ptr;
		};
} KWDENT;

inline	static	int	__util$lookup_key	(
			char	*src,
			int	srclen,

		KWDENT *	ktbl,
			int	ktblsz,

		KWDENT	**	kwd,

			int	flags
			)
{
int	status = STS$K_ERROR, len;
KWDENT *	k = ktbl;

	for ( k = ktbl, len = 0; ktblsz; ktblsz--, k++)
		{
		/* Exact comparing or abbreviated ? */
		if ( !(flags & util$K_LOOKUP_ABBREV) )
			if ( srclen != k->kwd->len )
				continue;

		/*
		* We performs a comparing only in case when a compare length is
		* more then at previous step.
		*/
		if ( len < $MIN(srclen, k->kwd->len) )
			len = $MIN(srclen, k->kwd->len);
		else	continue;


		/* Comparing ... */
		status = (flags & util$K_LOOKUP_NCASE) ?
#ifdef	WIN32

			_strnicmp
#else
			strncasecmp
#endif
			(src, k->kwd->sts, len) : memcmp(src, k->kwd->sts, len);

		if ( status )
			continue;
		}

	/* No matching ... */
	if ( status )
		return	STS$K_ERROR;

	*kwd = k;
}



/**
 * @brief __UTIL$UUID2STR - convert UUID from binary form to the human readable text string.
 *		implied zero byte (NIL, '\0') at end of the string.
 *
 * @param uuid: UUID to be converted
 * @param buf:	a buffer to accept text string
 * @param bufsz:a size of the output buffer
 *
 * @return -	length of the data in the output buffer.
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




/**
 * @brief __UTIL$FILL - Fill destination buffer with a pattern data.
 *
 * @param dst	- destination buffer to be filled
 * @param dstsz	- a size of the destination buffer
 * @param src	- a buffer with the pattern data
 * @param srcsz	- a size of the pattern data
 * @return
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

	while ( dstsz )
		{
		fillen = $MIN(srcsz, dstsz);
		memcpy(_dst, src, fillen);

		dstsz	-= fillen;
		_dst	+= fillen;


		}

	return	STS$K_SUCCESS;
}



/**
 * @brief __UTIL$STRXOR - XORing octet source string with a given key , result is in destination buffer.
 *	<ctx> - keep context between consecutive calls. Should be set to -1 at first call.
 *
 * @param key	- a buffer with the key
 * @param keysz	- a length of the key
 * @param src	- a buffer with the data to be XOR-ed
 * @param srcsz	- a lenth of the source data
 * @param dst	- a buffer to accept XOR-ed data
 * @param dstsz	- a size of the output buffer
 * @param ctx	- an internal context , can be NULL is is not used
 * @return
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
		*ctx = (kp - ((unsigned char *) key));		/* Store a current key's position */

	return	STS$K_SUCCESS;
}




/*	LIB$MOVC5 - Execute MOVC5 instruction
**++
** FUNCTIONAL DESCRIPTION:
**
**	LIB$MOVC5 makes the VAX MOVC5 instruction available as
**	a callable procedure.
**
**	The source is moved to the destination.  If the destination is
**	longer than the source, the highest address bytes of the
**	destination are replaced by the fill argument.  If the
**	destination is shorter than the source, the highest
**	addressed bytes of the source are not moved.  The operation is
**	such that overlap of the source and destination does not
**	affect the result.
**
**	For more information, see the VAX-11 Architecture Handbook.
**
** CALLING SEQUENCE:
**
**	status.wlc.v = LIB$MOVC5 (src_len.rwu.r, source.rz.r, fill.rb.r,
**				  dst_len.rwu.r, dest.wz.r)
**
** FORMAL PARAMETERS:
**
**	src_len		; The length of source in bytes.  Passed
**			; by reference.  The maximum length is 65535.
**
**	source		; The source to move from.  Passed by reference.
**
**	fill		; The fill character.  Passed by reference.
**
**	dst_len		; The length of dest in bytes.  Passed by
**			; reference.  The maximum length is 65535.
**
**	dest		; The destination to move to.  Passed by
**			; reference.
**
**
** IMPLICIT INPUTS:
**
**	NONE
**
** IMPLICIT OUTPUTS:
**
**	NONE
**
** COMPLETION STATUS:
**
**	SS$_NORMAL	Procedure successfully completed.
**
** SIDE EFFECTS:
**
**	NONE
**
**--

				LIB$MOVC5                       19-NOV-2004 08:39:23  Compaq C V6.5-001-48BCD           Page 4
								15-DEC-1994 11:02:13  $1$DGA1016:[LIBRTL_2.SRC]LIBMOVC5.C;1
*/

static inline int	__util$movc5	(
		unsigned short  *srclen,	/* pointer to source length	*/
		char            *source,	/* pointer to source 		*/
		unsigned char   *fill,		/* pointer to fill byte 	*/
		unsigned short  *dstlen,	/* pointer to destination length */
		char             *dest		/* pointer to destination	*/
			)
{
	/*
	*  if the destination is the same size as the source, or shorter
	*  then this is a simple move.
	*/

	if (*srclen >= *dstlen)
		memmove (dest, source, *dstlen);
	else	{
		/*
		 * destination is larger, do move and fill
		 */
		memmove (dest, source, *srclen);
		memset (&dest[*srclen], *fill, *dstlen - *srclen);
		}

	return STS$K_SUCCESS;
}


#ifdef	__TRACE__
//	#define	$TRACEBACK(expr)	{if ( (expr) ) __util$traceback ( #expr, (expr) ); }
#endif




/* Follow stuff are for debug/development purpose only	*/

typedef	struct __sym_rec__ {
	unsigned long long	val;		/* binary value or mask		*/
	unsigned char		len, *sym;	/* ASCII counted string		*/
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
		unsigned char	*out,
			int	 outsz
			)
{
int	outlen = 0;

	for ( ; sym->val && sym->sym; sym++)
		{
		if ( !(mask & sym->val) )
			continue;

		if ( outsz > sym->len )
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
		unsigned char	*out,
			int	 outsz
			)
{
int	outlen = 0;

	for ( sym = sym; sym->len && sym->sym; sym++)
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








#define	$SYM_REC_INI(s) {s, sizeof(#s)-1, #s}
#define	$SYM_REC_EOL	{0, 0, NULL}





















#ifndef	WIN32
#pragma GCC diagnostic pop
#endif

#pragma	pack (pop)

#ifdef __cplusplus
}
#endif

#endif	/*	__util$UTILS__	*/
