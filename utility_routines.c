#define	__MODULE__	"UTIL$"
#define	__IDENT__	"V.01-33ECO2"
#define	__REV__		"1.33.2"


/*
**  Abstract: General purpose and utility routines
**
**  Author: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  Creation date: 4-SEP-2017
**
**  Modification history:
**
**	25-SEP-2017	RRL	Added __util$log2buf(): format a message into the given buffer.
**
**	 5-OCT-2017	RRL	Added __util$syslog() : format and send a message to SYSLOG server;
**				recoding __util$setlogfile -> util$deflog() to accept a SYSLOG IP address
**				as argument;
**
**	 6-OCT-2017	RRL	Adopted __util$syslog() for M$ Windows.
**
**	12-OCT-2017	RRL	Added conditional blocks on the __SYSLOG__;
**				other changes to improve stability of code;
**
**	27-APR-2018	RRL	Changed handling of the OPTS$K_INT to allow accept decimals and hexadecimals,
**				replaced atoi() with strtoul().
**
**	28-MAY-2018	RRL	Correct text of message.
**
**	 6-AUG-2018	SYS	Added a support for 64-bits long in the routines related option's processing.
**
**	10-NOV-2018	RRL	Recoded logging related routines to use 'fd' instead of FILE;
**
**	12-NOV-2018	RRL	Changed __util$rewindlog() to truncate file at begin after rewinding.
**
**	23-NOV-2018	RRL	Fixed lseek()->29 on STDOUT_FILENO in the __util$rewindlogfile();
**
**	23-APR-2019	RRL	Recoded __util$readconfig() to allow using similar keywords  like:
**				/NSERVER
**				/NSERVER6
**
**	 4-JUN-2019	RRL	Improved output buffer security in the $trace(), $log(), $logd(), $log2buf() routines.
**
**	25-JUN-2019	ARL	Special changes for Android;
**			RRL	Small changes in the  __utility$trace();
**
**	 8-JUL-2019	RRL	Improved code quality in the routines processing of configuration options.
**
**	 5-OCT-2019	RRL	Adopted gettid() for APPLE/OSX
**
**	30-DEC-2019	RRL	Improved error handling during processing _CONF option.
**
**	19-MAR-2020	RRL	Added O_APPEND flag in call of open() in the __util$deflog().
**
**	 7-MAY-2020	RRL	Fixed incorrect value of 'mode' argument of open() under Windows.
**
**	17-MAR-2021	RRL	V.01-01 :  Added a set of routines to mimic to $GETMSG/$PUTMSG service routines.
**
**	18-MAR-2021	RRL	V.01-01ECO1 : Adopting for WIN32 .
**
**	25-APR-2021	RRL	Added has been lost __util$out()
**
**	 7-APR-2021	RRL	V.01-01ECO2 : Fixed $PUTMSGD().
**
**	10-AUG-2021	RRL	V.01-01ECO3 : Added missing dup2 for STDERR/STDOUT_FILENO;
**				improved BUFOVF checking;
**
**	12-AUG-2021	RRL	V.01-01ECO4 : Fixed incorrect length of the buffer in the __util$trace().
**
**	 8-SEP-2021	RRL	V.01-01ECO5 : Win32 related corrections.
**				V.01-01ECO5 : Remove useless stuff (getcpu()), some other corrections.
**
**	28-SEP-2021	Anton	V.01-01ECO6 : Changes to allow to use an external message handler for accept messages;
**			RRL	cleanup from unused stuff, small other fixes.
**
**	27-OCT-2021	RRL	V.01-01ECO7 : Added some more GCC related stuff to compile w/o warnings.
**
**	14-JUN-2022	RRL	V.01-02 : Added some routines.
**
**	10-NOV-2021	RRL	V.01-02ECO1 : Reduced compilation warnings
**
**	 9-DEC-2022	RRL	V.01-02ECO2 : Reduced GCC's compilation warnings
**
**	14-FEB-2023	RRL	V.01-03 : __util$vfao -  like SYS$FAO from DIGITAL VMS-World!
**
**	14-FEB-2023	RRL	V.01-04 : Added SYS_FAOL from Dave Jonse OSU WEB with some cosmetic changes for more readability
**
**	24-MAY-2024	RRL	V.01-04ECO1 : Fixed possible buffer overrun in the  __util$dumphex()
**
**	24-APR-2026	RRL	V.01-05 : Added support for parsing CLI option started with "--",
**				--trace --logfile ...
**
**	27-JUL-2026	RRL	V.01-06 : Memory-safety fixes:
**				- __util$uint32_2_dec() : fixed 1-byte heap overflow (pointer was
**				  set one past the buffer), render 0 as "0", keep room for the NUL;
**				- __util$faohex() : do not read past <a_src> for short input, check
**				  <a_outsz> before writing (was a fixed-offset overrun);
**				- __util$vfao() : reserve one octet for the terminating NUL;
**				- s_fmt_pad() : fixed write through an uninitialized pointer on the
**				  buffer-overflow path;
**				- s_fmt_unsigned() : digit_stack is now LONG (was int) - fixes
**				  truncation and signed overflow for 64-bit !xQ/!xL values;
**				- s_fmt_time() : implemented !%D / !%T (was returning an uninitialized
**				  value and corrupting the output length);
**				- __util$faol() : accept the L/Q size specifiers for !U/!X/!O/!Z;
**				- __util$readconfig() : fixed inverted "no config file" guard
**				  (rejected every valid file name), report an error instead of reading stdin;
**				- made s_severity[]/s_defmsgfao[] static, O_CLOEXEC on the log file,
**				  strtoul() for the PORT option, int (not size_t) length in __util$putmsgd();
**				- silenced all -Wsign-compare in __util$vfao()/s_fmt_signed() (MIN_INT
**				  is now a signed INT_MIN), fixed $STS_ERROR()/$DEFMSG() macros in the header;
**				- __util$readconfig() : reject an input longer than the option name so a
**				  typo like "NSERVERXYZ" no longer matches "NSERVER";
**				- converted the remaining Doxygen comment blocks to the VMS style and
**				  corrected spelling throughout.
**
**	27-JUL-2026	RRL	V.01-07 : Performance - __util$crc32c() (which actually computes the
**				standard IEEE CRC-32, not Castagnoli - see note at the routine):
**				- portable slice-by-8 core, ~4.5x the byte-at-a-time loop, endian-neutral;
**				- ARMv8 hardware path (crc32b/crc32x) under __ARM_FEATURE_CRC32, bit-identical
**				  output (build RK targets with -march=armv8-a+crc / -mcpu=cortex-a35|a55).
**
**	28-JUL-2026	RRL	V.01-08 : Removed the legacy FAO facility (__util$fao/__util$faol
**				and their static helpers __util$vfao/s_output_directive/s_fmt_*); the
**				FAO service is now provided by the starlet module as __util$fao/
**				__util$faol. Renamed the static __msgcmp() to s_msgcmp() so that every
**				static routine carries the s_ prefix. __util$faohex() is retained.
**				Dropped the vestigial #if __util_DEBUG__ scratch/test block.
**
**	30-JUL-2026	RRL	V.01-09 : __util$remqent() - fixed removal of an entry that is the head,
**				the tail or the only element of the queue: re-point __QUEUE head/tail when a
**				neighbour is absent instead of dereferencing a NULL left link.
**
**	30-JUL-2026	RRL	V.01-10 : Documentation pass - gave every routine a VMS-style
**				**++ / **-- header (FUNCTIONAL DESCRIPTION / FORMAL PARAMETERS / IMPLICIT
**				INPUTS-OUTPUTS / RETURN VALUE) and added in-code comments to the sparsely
**				commented routines. No functional change.
**				Also documented the __MAIN_FOR_DEBUG__ demo main and normalised the
**				source to LF line endings (see .gitattributes).
**
**	01-AUG-2026	RRL	V.01-11 : __util$log()/__util$log2buf() validated the severity
**				against [STS$K_WARN..STS$K_ERROR], which rejected STS$K_FATAL (4);
**				it was clamped to STS$K_UNDEF (odd, reads as success).  Widened the
**				accepted range to STS$K_FATAL.
**
**	01-AUG-2026	RRL	V.01-12 : Routed the nine best-effort log writes through a new
**				s_util$logwrite() helper that consumes write()'s result, so the
**				module builds clean under -O2 / -D_FORTIFY_SOURCE (warn_unused_result).
**
**	01-AUG-2026	RRL	V.01-13 : Fixed two stack buffer overflows of the same shape:
**				__util$out() and __util$putmsg() appended the terminating <LF> at
**				the offset (v)snprintf() had REQUIRED rather than the one it had
**				actually WRITTEN, so a formatted record longer than the buffer wrote
**				the <LF> past its end and then handed the inflated length to write().
**				__util$logd() and __util$log2buf() already clamped; the same clamp is
**				applied here. Tightened three further clamps from sizeof(out) - 1 and
**				UTIL$SZ_OUTBUF down to UTIL$SZ_OUTBUF - 1: the arrays carry 8 octets of
**				padding which (v)snprintf() never writes, so a truncated record used to
**				emit up to 8 uninitialized stack octets into the log.
**				See tests/test_log_overflow.c.
**
**	01-AUG-2026	RRL	V.01-14 : Hardened __util$log2buf() against overflow of the CALLER's
**				buffer, on two counts: (a) it clamped the length to outsz and wrote the
**				terminator at __outbuf[outsz], one past the end, when a record filled
**				the buffer - clamp to outsz-1; (b) when the timestamp prefix alone was
**				longer than outsz, snprintf() returned a length > outsz and the follow-
**				up vsnprintf() was handed a negative (hence huge) size, writing far past
**				the buffer - the prefix length is now clamped before the append and a
**				outsz < 1 guard was added. Caught by tests/test_log_overflow.c.
**
**	01-AUG-2026	RRL	V.01-15 : The p_cb_log_f diversion hook is honoured by the logging
**				routines as well. It was consulted by __util$out(), __util$trace() and
**				__util$dumphex() only, while __util$log(), __util$logd(), __util$putmsg()
**				and __util$putmsgd() wrote to g_logoutput regardless - so an application
**				that installed the hook could divert its tracing and its raw output but
**				not its $LOG records, which kept landing on the standard output. That is
**				a hazard for a utility whose standard output carries data.
**
**	01-AUG-2026	RRL	V.01-16 : Removed the p_cb_log_f diversion hook and its global pointer
**				entirely; every logging routine now writes straight to g_logoutput.
**				Output is redirected with __util$deflog() (file or SYSLOG host) instead.
**
**	01-AUG-2026	RRL	V.01-17 : Added __util$setlogfd(): point the logging at an already-
**				open descriptor without opening a file or seizing stdout/stderr, as
**				__util$deflog() does. Lets a utility whose standard output carries data
**				send its log to the standard error with one call and no dup2() dance.
**
**	 3-AUG-2026	RRL	V.01-18 : Added the FAO variants of the message signalling:
**				__util$putmsg_fao() and __util$putmsgd_fao(), reached through the
**				$PUTMSG_FAO() macro. They look a catalogue record up by its
**				condition value like __util$putmsg()/__util$putmsgd(), but format
**				the record text as an FAO control string via __util$faol() (the
**				starlet FAO service) rather than as a printf format - so a record
**				may use !UL/!UQ/!AS/!AD and friends. starlet.h is now included by
**				this module for the FAO API.
**
**	 3-AUG-2026	RRL	V.01-19 : $PUTMSG_FAO() now applies the (fao_prm_t) cast to each
**				parameter itself (FAO$_EACH machinery in the header), so a caller writes
**				the value plain - $PUTMSG_FAO(sts, 2, l_seq, l_disp) - and can no longer
**				forget the cast, which was silent undefined behaviour. The explicit
**				count is unchanged; 0..16 parameters are handled.
**
**	 3-AUG-2026	RRL	V.01-20 : Under NDEBUG the no-op $IFTRACE() now references its condition
**				via sizeof (used, but not evaluated), so a trace-only local such as
**				cli_routines' qlog no longer draws -Wunused-variable in a release build.
**
**	 3-AUG-2026	RRL	V.01-21 : $PUTMSG_FAO() now derives the parameter count from the list
**				itself (FAO$_NARG), so the caller writes neither the count nor the casts:
**				$PUTMSG_FAO(sts, l_seq, l_disp). A count that is never written by hand
**				cannot disagree with the list beside it, nor send the routine reading
**				parameters that were never passed.
**
**	 7-AUG-2026	RRL	V.01-22 : s_msgcmp() compared the whole condition value, so a record was
**				found only when the signalled severity matched the registered one. It now
**				compares the message number ($MSG, severity bits dropped), so one record
**				serves any severity - a status may be signalled INFO in one place and
**				WARNING in another. The severity shown/returned is the caller's ($SEV);
**				a message number is expected once per facility catalogue.
**
**	 7-AUG-2026	RRL	V.01-23 : s_msgcmp() now compares facility then message number (still
**				severity-independent), so records of different facilities cannot collide.
**				__util$putmsg_fao()/__util$putmsgd_fao() prefix a found record with the
**				caller's severity - %<S>- taken from $SEV(sts) - so one record shows its
**				INFO/WARNING/... face.
**
**	 7-AUG-2026	RRL	V.01-24 : Restored the facname field in EMSG_RECORD_DESC (the field only,
**				not __util$fmtmsg), so a signalled record is now framed with the full
**				OpenVMS envelope %<facility>-<S>-<text>: s_util$facname() supplies the
**				name (or NONAME), the severity is the caller's ($SEV).
**
**	13-AUG-2026	RRL	V.01-25 : The log-record prefix is now rendered by s_util$prefix() -
**				byte-identical to the former snprintf of "%02u-%02u-%04u ..." but ~20x
**				cheaper: the date-time is reformatted only when the second changes
**				(a per-thread cache), the digits are laid out by hand, and the thread
**				id is fetched once per thread instead of a syscall per record. All nine
**				prefix sites (putmsg[d][_fao], logd, log, log2buf, trace, dumphex) use
**				the helper; the suffixes ([mod:line], %<fac>-<S>-) stay as snprintf.
**				End-to-end: putmsg_fao 1153 -> 259 ns, $LOG 1216 -> 302 ns. En passant:
**				dumphex's WIN32 branch referenced _tm/now instead of l_tm/l_now (would
**				not compile there) - gone with the block. NB: after a fork() the child
**				inherits the cached tid - see s_util$prefix() notes.
**
**	14-AUG-2026	RRL	V.01-26 : __util$pattern_match() rewritten from the recursive matcher
**				to the iterative one with a single backtrack point. Same semantics ('*'
**				a run, '%' one octet; 500000 random cases agree with the old algorithm,
**				see tests/test_pattern_match.c), but bounded by O(|s|*|p|): the pattern
**				a*a*a*a*a*b over 200 octets of 'a' went from 8.6 SECONDS to 318 ns, and
**				the typical cases got 1.6-3.6x faster (15-26 ns). No recursion - no
**				stack hazard on hostile patterns from a config or the CLI.

**	14-AUG-2026	RRL	V.01-26ECO1 : the session's routines (s_msgcmp, s_util$prefix,
**				s_util$facname) brought to the house routine-body template - one
**				parameter per line, the closing parenthesis on its own, a_-prefixed
**				parameters. Layout only, no functional change.
**
**	29-AUG-2026	RRL	V.01-27 : Added $LOG_FAO - the FAO twin of $LOG: the body is an FAO
**				control string (!UL/!AZ/!AS/!XL/!%I/...) formatted by __util$faol(),
**				with the $PUTMSG_FAO ergonomics (every parameter cast to fao_prm_t and
**				the count derived by the macro). __util$log_fao() mirrors __util$log()
**				(envelope, sanity clamp, STS$M_SYSLOG path reusing the formatted body),
**				__util$logd_fao() mirrors __util$logd() ([mod\func:line] debug face).
**				See tests/test_log_fao.c.
**
**	31-AUG-2026	RRL	V.01-28 : The facility name is stamped into every EMSG_RECORD (new
**				trailing facname field) by __util$inimsg(), so __util$putmsg_fao()/
**				__util$putmsgd_fao() read it off the found record instead of walking
**				the descriptor list a SECOND time per message (the list had already
**				been walked to find the record). s_util$facname() is gone with the
**				walk. Existing positional record initializers stay valid - the field
**				is trailing and filled at registration. See tests/test_putmsg_fao.c.
**
**	 1-SEP-2026	RRL	V.01-29 : __util$pattern_match() accepts '?' as an alias of '%'
**				(exactly one octet) - the DOS/glob-style spelling next to the VMS
**				one. NB: a literal '?' in a pattern is a wildcard from now on. The
**				fuzz reference and vectors extended - tests/test_pattern_match.c.
**
**	10-SEP-2026	RRL	V.01-30 : Added __util$base64() / __util$unbase64() - RFC 4648
**				encoding and decoding with the bounded-buffer calling sequence of
**				the family (src, srclen, dst, dstsz, &dstlen). BroProxy and
**				BroAgent carried these two under the same names in a private
**				BASE64.H each; the library is where they belong.  The decoder
**				is strict where the old one was not: an octet outside the
**				alphabet, a '=' in the middle, or a dangling sextet is an error
**				rather than garbage out.  tests/test_base64.c pins it.
**
**	11-SEP-2026	RRL	V.01-30ECO1 : Fixed $MSG() in the header - it shifted the message
**				number down but never masked the facility off, so it was not the
**				inverse of $DEFMSG(): for a non-zero facility it returned
**				<facility> << 13 | <number>. s_msgcmp() survived it only because
**				it compares the facility first and the ordering came out right by
**				accident; the s_defmsgfao line of an unregistered status printed the
**				bogus number. tests/test_status_convention.c now pins the layout -
**				$FAC/$MSG/$SEV must invert $DEFFAC/$DEFMSG/$DEFSEV field for field.
**
**	19-SEP-2026	RRL	V.01-31 : EMSG_RECORD gained a trailing mnemonic field - the real
**				VMS mnemonic (e.g. "CONFOK"), which a $XXX_MSGENT table stringises
**				from its own code (#a_sts, e.g. "ACST$_CONFOK") and __util$inimsg()
**				trims to the part after the facility's own "$_". __util$putmsg_fao()
**				and __util$putmsgd_fao() glue it onto the envelope as
**				%FAC-S-MNEMONIC, when a record carries one; a caller-built
**				EMSG_RECORD that never set the field (e.g. a test harness's own
**				table) is left on the plain %FAC-S- envelope this had before the
**				field existed - the same trailing-field precedent V.01-28 set for
**				facname. __util$putmsg()/__util$putmsgd() (the non-FAO pair) are
**				untouched: they never built the %FAC-S- envelope for a found
**				record to begin with. See tests/test_putmsg_fao.c, unchanged and
**				still passing on the NULL-mnemonic fallback.
**
**	 3-OCT-2026	RRL	V.01-32 : __util$crc32c() picks its engine at run time, on the first
**				call: PCLMULQDQ folding on x86-64 (Intel's method, as the Linux
**				kernel and zlib-ng have it for this polynomial), the CRC32
**				instructions on ARMv8 when AT_HWCAP says HWCAP_CRC32, the slice-by-8
**				tables elsewhere. The value is the same IEEE CRC-32 as before, bit
**				for bit - only faster: 0.62 -> several GB/s on a Xeon E5-2680 v4.
**				The ARMv8 path used to depend on __ARM_FEATURE_CRC32, which the
**				default -march of Debian/Armbian does not define, so those hosts ran
**				the tables. STARLET_CRC32=SOFTWARE forces the tables. Asked for by
**				VBACKUP, which checksums every block and every file it saves.
**				tests/test_crc32.c pins every engine against a bitwise reference.
**
**	 4-OCT-2026	RRL	V.01-33 : Added __util$rotatelogfile() - once the log file grows past
**				a limit it is renamed to <file>.1 (the older ones shift up to
**				<file>.<keep>, the oldest dropped) and a fresh one is opened on the
**				same descriptor, stdout and stderr with it. Nothing is copied, so a
**				slow disk is not stalled the way logrotate's copytruncate stalls it,
**				and nothing is lost the way __util$rewindlogfile() loses it. keep = 0
**				truncates in place. __util$deflog() remembers the file name for it
**				and, called again, closes the file it opened before instead of
**				leaking the descriptor. Asked for by ACS. tests/test_logrotate.c.
**
**	 6-OCT-2026	RRL	V.01-33ECO1 : The log-record prefix takes the thread id with
**				syscall(SYS_gettid) itself instead of the __gettid() wrapper of
**				utility_routines.h - the wrapper is compiled out under ANDROID,
**				so the library did not build with the Android NDK. Still once
**				per thread (s_tid).
**
**	20-OCT-2026	RRL	V.01-33ECO2 : __util$syslog() on Windows: the address of the
**				WSAMSG cast to LPSOCKADDR - GCC 14 (MSYS2) takes the
**				incompatible pointer for an error.
**
*/


#ifdef	_WIN32
#define _CRT_SECURE_NO_WARNINGS	1
#define	WIN32_LEAN_AND_MEAN

#include	<winsock2.h>
#include	<windows.h>
#include	<ws2tcpip.h>

#include	<sys/timeb.h>
#include	<io.h>
#endif

#include	<stddef.h>
#include	<stdio.h>
#include	<stdarg.h>
#include	<time.h>
#include	<ctype.h>
#include	<string.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>


#ifdef  ANDROID
#include	<android/log.h>
#endif


#define	UTIL$SZ_OUTBUF	2048

#ifndef	_WIN32
#include	<unistd.h>
//#include	<execinfo.h>
#include	<arpa/inet.h>
#include	<syslog.h>

#define	UTIL$T_PID_FMT	"%6d "
	#define	TIMSPECDEVIDER	(1024*1024)	/* Used to convert timespec's nanosec tro miliseconds */

#elif	_WIN32
	#define	gettid()	GetCurrentThreadId()
	//#define	gettid()	GetCurrentProcessId()
	#define	TIMSPECDEVIDER	1024		/* Used to convert timeval's microseconds to miliseconds */
	#define	UTIL$T_PID_FMT	"%6d "
#else
	#define	gettid()	(0xDEADBEEF)
#endif

#ifndef	__TRACE__
#define		__TRACE__	/* Enable $TRACE facility macro				*/
#endif

#include	"utility_routines.h"
#include	"starlet.h"					/* FAO service for the _FAO message variants	*/


#ifndef	WIN32
	#pragma	GCC diagnostic push

	#pragma GCC diagnostic ignored  "-Wformat"
	#pragma GCC diagnostic ignored  "-Wunused-result"
	#pragma GCC diagnostic ignored  "-Wunused-value"
#endif

#ifdef _WIN32

#ifndef	STDERR_FILENO
#define	STDERR_FILENO	2
#endif

#ifndef	STDIN_FILENO
#define	STDIN_FILENO	0
#endif

#ifndef	STDOUT_FILENO
#define	STDOUT_FILENO	1
#endif


#endif // _WIN32

static int	g_logoutput = STDOUT_FILENO;				/* Default descriptor for default output device		*/
static char	s_logpath [1024];						/* The file __util$deflog() opened, for rotation	*/
static size_t	s_logpathlen;							/* Its length, 0 - the log is not a file		*/
static long	s_logrotating;							/* A rotation is under way (another thread skips)	*/

#ifdef	_WIN32
#define	$LOGROT_CLAIM()		InterlockedExchange(&s_logrotating, 1)
#define	$LOGROT_RELEASE()	InterlockedExchange(&s_logrotating, 0)
#else
#define	$LOGROT_CLAIM()		__atomic_exchange_n(&s_logrotating, 1, __ATOMIC_ACQUIRE)
#define	$LOGROT_RELEASE()	__atomic_store_n(&s_logrotating, 0, __ATOMIC_RELEASE)
#endif



struct sockaddr_in slogsock;						/* SYSLOG Host socket					*/


static EMSG_RECORD_DESC	*emsg_record_desc_root;				/* A root to the message records descriptor		*/

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Best-effort write of a formatted record to the log descriptor.  The result
**	is deliberately discarded - there is nothing sensible to do when the log
**	write itself fails - but it is consumed so that write()'s warn_unused_result
**	does not fire under -D_FORTIFY_SOURCE / -O2.
**
**  FORMAL PARAMETERS:
**
**	a_fd	The log file descriptor
**	a_buf	The record to write
**	a_len	Its length in octets
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	NONE.
**--
*/
static	inline void	s_util$logwrite	(int a_fd, const void *a_buf, size_t a_len)
{
	if ( 0 > write (a_fd, a_buf, a_len) )
		return;			/* best-effort: nothing to do on failure	*/
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Comparison routine for the message catalogue. Orders two EMSG_RECORD
**	entries by their condition/message code, so that qsort() can sort the
**	catalogue and bsearch() can look a message up by that code.
**
**  FORMAL PARAMETERS:
**
**	a	Address of the first  EMSG_RECORD
**	b	Address of the second EMSG_RECORD
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	Negative, zero or positive as a->sts is less than, equal to or
**	greater than b->sts.
**--
*/
static	int	s_msgcmp (
		const void	*a_m1,
		const void	*a_m2
		)
{
const EMSG_RECORD *m1 = a_m1, *m2 = a_m2;
int	l_c;


	/*
	 * Compare by facility, then by message number, with the severity bits
	 * dropped. A record is thus found regardless of the severity carried by the
	 * signalled status - one status may be signalled INFO in one place and
	 * WARNING in another from a single record - while records of different
	 * facilities never collide. The severity shown is the caller's ($SEV of the
	 * status). A message number is expected to appear once in a facility's catalogue.
	 */
	l_c = (int) $FAC(m1->sts) - (int) $FAC(m2->sts);
	return	l_c ? l_c : ( (int) $MSG(m1->sts) - (int) $MSG(m2->sts) );
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Initialise a message catalogue: sort the caller's array of message
**	records by code so that __util$getmsg() can binary-search it, and link
**	the descriptor into the active catalogue list.
**
**  FORMAL PARAMETERS:
**
**	msgdsc	Address of the message-records descriptor (array + count)
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Links <msgdsc> into the module-static emsg_record_desc_root list.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on success; STS$K_WARN / condition code otherwise.
**--
*/
unsigned	__util$inimsg (
		EMSG_RECORD_DESC	*a_msgdsc
		)
{
EMSG_RECORD_DESC *l_md;
unsigned l_i;

	if ( !a_msgdsc )
		return	STS$K_WARN;

	/* At first level we try to find the message records descriptor by using facility number */
	for  (l_md = emsg_record_desc_root; l_md; l_md = l_md->link)
		if ( l_md->facno == a_msgdsc->facno)
			return	STS$K_WARN;


	qsort(a_msgdsc->msgrec, a_msgdsc->msgnr, sizeof(EMSG_RECORD), s_msgcmp);

	/* Stamp the facility name into every record, so the signalling routines
	** read it off the found record instead of walking the descriptor list
	** a second time (the descriptor was already walked to FIND the record).
	*/
	for ( l_i = 0; l_i < a_msgdsc->msgnr; l_i++ )
		{
		const char	*l_mn;

		a_msgdsc->msgrec[l_i].facname = a_msgdsc->facname;

		/*  A $XXX_MSGENT table stringises its own code (#a_sts) into this
		**  field at compile time, e.g. "ACST$_CONFOK"; the real mnemonic is
		**  the part after the facility's own "$_" - the invariant every
		**  FAC$_NAME code follows, not merely "after the first underscore".
		**  A caller-built EMSG_RECORD that never set the field is left NULL.
		*/
		if ( a_msgdsc->msgrec[l_i].mnemonic && (l_mn = strstr (a_msgdsc->msgrec[l_i].mnemonic, "$_")) )
			a_msgdsc->msgrec[l_i].mnemonic = l_mn + 2;
		}

	a_msgdsc->link = emsg_record_desc_root;
	emsg_record_desc_root = a_msgdsc;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Look up a message record by its condition/message code in the
**	catalogue registered with __util$inimsg(). The facility number selects
**	the descriptor, then the record is found by binary search.
**
**  FORMAL PARAMETERS:
**
**	sts	Condition code / message number to find
**	outmsg	Receives the address of the matching EMSG_RECORD
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the module-static emsg_record_desc_root list.
**
**  RETURN VALUE:
**	STS$K_SUCCESS - <outmsg> holds the found record;
**	condition code otherwise.
**--
*/
unsigned	__util$getmsg	(unsigned sts, EMSG_RECORD **outmsg )
{
unsigned	facno;
EMSG_RECORD_DESC *msgdsc;
EMSG_RECORD *msgrec;

	/* At first level we try to find the message records descriptor by using facility number */
	facno = $FAC(sts);
	for  (msgdsc = emsg_record_desc_root; msgdsc; msgdsc = msgdsc->link)
		if ( msgdsc->facno == facno)
			break;

	if ( !msgdsc )
		return	STS$K_ERROR;		/* RNF - Record-Not-Found */

	/* We found message records descriptor for the facility,
	 * so we can try to find the message record with the given <msgno>
	 */
	if ( !(msgrec = bsearch(&sts, msgdsc->msgrec, msgdsc->msgnr, sizeof(EMSG_RECORD), s_msgcmp)) )
		return	STS$K_ERROR;		/* RNF */

	*outmsg = msgrec;

	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Format a catalogue message by its condition/message code (looked up via
**	__util$getmsg()) together with any FAO/printf-style arguments, and write
**	the result to the current log destination.
**
**  FORMAL PARAMETERS:
**
**	sts	Condition/message code selecting the catalogue record
**	...	Arguments for the message text, per its format
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the message catalogue; writes to the log destination.
**
**  RETURN VALUE:
**	The <sts> argument, so the call may be used in a return statement.
**--
*/

static const char s_severity[STS$K_MAX]= { 'W', 'S', 'E', 'I', 'F', '?', '?', '?'};
static const char s_defmsgfao[] = {"NONAME-%c-NOMSG, Message number: %08X [fac: %#x/%d, sev: %#x/%d, msgno: %#x/%d]"};

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Format the standard log-record prefix - "DD-MM-YYYY HH:MM:SS.msc <tid> ",
**	byte-identical to the former snprintf of
**	"%02u-%02u-%04u %02u:%02u:%02u.%03u " UTIL$T_PID_FMT - but at a fraction
**	of the cost: the date-time part up to the '.' is reformatted only when the
**	second changes (a per-thread cache), the digits are emitted by hand, and
**	the thread id is fetched from the kernel once per thread instead of a
**	syscall per record.
**
**  FORMAL PARAMETERS:
**
**	a_out	- address of the output buffer, at least 48 octets
**
**  IMPLICIT INPUTS:
**	Per-thread caches of the current second's text and of the thread id.
**
**  SIDE EFFECTS:
**	NB: after a fork() the child inherits the parent's cached thread id;
**	a process that forks after its first log record and cares about the tid
**	column should reset it via pthread_atfork() or re-exec.
**
**  COMPLETION STATUS:
**	Length of the prefix written to <a_out>, octets.
**--
*/
#if defined(_WIN32)
#define	UTIL$TLS	__declspec(thread)
#else
#define	UTIL$TLS	__thread
#endif

static	int	s_util$prefix (
		char	*a_out
		)
{
struct timespec l_now;
unsigned l_val;
int	l_n, l_k, l_pad;
char	l_tmp [16];
static UTIL$TLS time_t	s_sec = (time_t) -1;
static UTIL$TLS char	s_datim [20];		/* "DD-MM-YYYY HH:MM:SS."	*/
static UTIL$TLS unsigned s_tid;

	s___time (&l_now);

	if ( l_now.tv_sec != s_sec )		/* A new second - reformat the cached date-time part */
		{
		struct tm l_tm;

#ifdef	WIN32
		localtime_s (&l_tm, (time_t *) &l_now);
#else
		localtime_r ((time_t *) &l_now, &l_tm);
#endif
		s_datim[0] = (char) ('0' + l_tm.tm_mday / 10);
		s_datim[1] = (char) ('0' + l_tm.tm_mday % 10);
		s_datim[2] = '-';
		s_datim[3] = (char) ('0' + (l_tm.tm_mon + 1) / 10);
		s_datim[4] = (char) ('0' + (l_tm.tm_mon + 1) % 10);
		s_datim[5] = '-';
		l_val = (unsigned) (1900 + l_tm.tm_year);
		s_datim[6] = (char) ('0' + l_val / 1000);
		s_datim[7] = (char) ('0' + (l_val / 100) % 10);
		s_datim[8] = (char) ('0' + (l_val / 10) % 10);
		s_datim[9] = (char) ('0' + l_val % 10);
		s_datim[10] = ' ';
		s_datim[11] = (char) ('0' + l_tm.tm_hour / 10);
		s_datim[12] = (char) ('0' + l_tm.tm_hour % 10);
		s_datim[13] = ':';
		s_datim[14] = (char) ('0' + l_tm.tm_min / 10);
		s_datim[15] = (char) ('0' + l_tm.tm_min % 10);
		s_datim[16] = ':';
		s_datim[17] = (char) ('0' + l_tm.tm_sec / 10);
		s_datim[18] = (char) ('0' + l_tm.tm_sec % 10);
		s_datim[19] = '.';

		s_sec = l_now.tv_sec;
		}

	memcpy (a_out, s_datim, 20);
	l_n = 20;

	/* The fraction, zero-filled to three digits as %03u did - the very same
	** (unsigned) tv_nsec / TIMSPECDEVIDER value as before
	*/
	l_val = (unsigned) l_now.tv_nsec / TIMSPECDEVIDER;
	l_k = 0;
	do	{
		l_tmp[l_k++] = (char) ('0' + l_val % 10);
		l_val /= 10;
		}
	while ( l_val );
	for ( ; l_k < 3; l_k++)
		l_tmp[l_k] = '0';
	while ( l_k )
		a_out[l_n++] = l_tmp[--l_k];

	a_out[l_n++] = ' ';

	/* The thread id, right-aligned to six columns as "%6d " did - fetched once per thread */
	if ( !s_tid )
#ifdef	_WIN32
		s_tid = (unsigned) gettid ();
#elif	defined(__APPLE__) || defined(__OSX__)
		s_tid = (unsigned) syscall (SYS_thread_selfid);
#else
		s_tid = (unsigned) syscall (SYS_gettid);	/* glibc, musl and Bionic alike: __gettid() is hidden under ANDROID */
#endif

	l_val = s_tid;
	l_k = 0;
	do	{
		l_tmp[l_k++] = (char) ('0' + l_val % 10);
		l_val /= 10;
		}
	while ( l_val );
	for ( l_pad = 6 - l_k; l_pad > 0; l_pad--)
		a_out[l_n++] = ' ';
	while ( l_k )
		a_out[l_n++] = l_tmp[--l_k];

	a_out[l_n++] = ' ';

	return	l_n;
}


unsigned	__util$putmsg
			(
		unsigned	sts,
			...
			)

{
va_list arglist;
char	out[UTIL$SZ_OUTBUF + 8];
int	olen, sev;
EMSG_RECORD *msgrec;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec <PID/TID> " prefix
	*/
	olen = s_util$prefix (out);				/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */

	if ( 1 & __util$getmsg(sts, &msgrec) )				/* Retrieve the message record */
		{
		va_start (arglist, sts);				/* Format text message */
		olen += vsnprintf(out + olen, UTIL$SZ_OUTBUF - olen, msgrec->text, arglist);
		va_end (arglist);
		}
	else	{							/* Format text message with the default FAO */
		sev = $SEV(sts);
		olen += snprintf(out + olen, UTIL$SZ_OUTBUF - olen, s_defmsgfao, s_severity[sev], sts,
			$FAC(sts), $FAC(sts), sev, sev, $MSG(sts), $MSG(sts));
		}

	/* (v)snprintf() returns the length it WOULD have written - clamp before appending */
	olen = $MIN(olen, UTIL$SZ_OUTBUF - 1);

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	s_util$logwrite (g_logoutput, out, olen);

	/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, " --- ", "%.*s", olen, out);
	#endif

	return	sts;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Format a catalogue message by code, with the call-site module, function
**	and line and any arguments, and write it to the log. Descriptor form
**	behind __util$putmsg().
**
**  FORMAL PARAMETERS:
**
**	sts	Condition/message code
**	__mod	Call-site module name
**	__fi	Call-site function name
**	__li	Call-site line number
**	...	Message arguments
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the message catalogue; writes to the log destination.
**
**  RETURN VALUE:
**	The <sts> argument.
**--
*/
unsigned	__util$putmsgd
			(
		unsigned	sts,
		const char *	__mod,
		const char *	__fi,
		unsigned	__li,
			...
			)

{
va_list arglist;
const char	lfmt [] = {"[%s:%u] "},
	mfmt [] = {"[%s\\%s:%u] "};
char	out[UTIL$SZ_OUTBUF + 8];
int	olen, sev;
EMSG_RECORD *msgrec;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec <PID/TID> " prefix
	*/
	olen = s_util$prefix (out);				/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */
	olen += __mod
		? snprintf (out + olen, UTIL$SZ_OUTBUF - olen, mfmt, __mod, __fi, __li)
		: snprintf (out + olen, UTIL$SZ_OUTBUF - olen, lfmt, __fi, __li);

	olen = $MIN(UTIL$SZ_OUTBUF - 1, olen);

	if ( 1 & __util$getmsg(sts, &msgrec) )				/* Retrieve the message record */
		{
		va_start (arglist, __li);				/* Format text message */
		olen += vsnprintf(out + olen, UTIL$SZ_OUTBUF - olen, msgrec->text, arglist);
		va_end (arglist);
		}
	else	{							/* Format text message with the default FAO */
		sev = $SEV(sts);
		olen += snprintf(out + olen, UTIL$SZ_OUTBUF - olen, s_defmsgfao, s_severity[sev], sts,
			$FAC(sts), $FAC(sts), (int) sev, (int) sev, $MSG(sts), $MSG(sts));
		}


	olen = $MIN(UTIL$SZ_OUTBUF - 1, olen);

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	s_util$logwrite (g_logoutput, out, olen);

	/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, __mod, "%.*s", olen, out);
	#endif

	return	sts;
}




/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The FAO variant of __util$putmsg(): look the catalogue record up by its
**	condition value and format its text as an FAO control string (via
**	__util$faol()) rather than a printf format, then write it to the log
**	destination with the usual timestamp/PID prefix. The record text may
**	use the FAO directives (!UL, !UQ, !AS, !AD, !XL ...); each variadic
**	parameter must be passed already cast to fao_prm_t.
**
**  FORMAL PARAMETERS:
**
**	sts	Condition/message code selecting the catalogue record
**	...	FAO parameters for the record text, each cast to fao_prm_t
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the message catalogue; writes to the log destination.
**
**  RETURN VALUE:
**	sts
**--
*/
unsigned	__util$putmsg_fao	(unsigned sts, int prmcnt, ...)
{
va_list	arglist;
char	out [UTIL$SZ_OUTBUF + 8], body [UTIL$SZ_OUTBUF];
fao_desc_t	bodydsc = $FAO_DESC(body);
unsigned short	bodylen = 0;
int	olen, sev, n;
fao_prm_t	prm [16];
EMSG_RECORD *msgrec;

	olen = s_util$prefix (out);				/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */

	olen = $MIN(UTIL$SZ_OUTBUF - 1, olen);

	/*
	**  Collect the FAO parameters into a list for __util$faol().  The
	**  count is taken from the caller rather than assumed: reading more
	**  arguments than were passed is undefined behaviour, and telling
	**  __util$faol() there are sixteen of them defeats its own guard
	**  against a control string with more directives than parameters -
	**  a mismatch between a catalogue entry and its caller would then
	**  format whatever the stack happened to hold.
	*/
	if ( prmcnt > (int) (sizeof(prm)/sizeof(prm[0])) )
		prmcnt = (int) (sizeof(prm)/sizeof(prm[0]));

	if ( prmcnt < 0 )
		prmcnt = 0;

	va_start (arglist, prmcnt);
	for ( n = 0; n < prmcnt; n++ )
		prm[n] = va_arg(arglist, fao_prm_t);
	va_end (arglist);

	if ( 1 & __util$getmsg(sts, &msgrec) )				/* Format the record text through FAO */
		{
		/*  Prefix the caller's severity - %<S>- - so one record can show its INFO/WARNING/... face.
		**  A record that carries a real VMS mnemonic (from its own $XXX_MSGENT table, trimmed by
		**  __util$inimsg()) gets it glued on as %FAC-S-MNEMONIC, ; one that never set the field
		**  (a caller-built EMSG_RECORD, e.g. a test harness's own table) keeps the plain %FAC-S-
		**  envelope this had before the field existed, body glued straight on - see V.01-28's
		**  facname precedent: a trailing field left NULL by an old initializer changes nothing.
		*/
		sev = $SEV(sts);
		olen += (msgrec->mnemonic && *msgrec->mnemonic)
			? snprintf(out + olen, UTIL$SZ_OUTBUF - olen, "%%%s-%c-%s, ",
				(msgrec->facname && *msgrec->facname) ? msgrec->facname : "NONAME", s_severity[sev], msgrec->mnemonic)
			: snprintf(out + olen, UTIL$SZ_OUTBUF - olen, "%%%s-%c-",
				(msgrec->facname && *msgrec->facname) ? msgrec->facname : "NONAME", s_severity[sev]);
		olen = $MIN(olen, UTIL$SZ_OUTBUF - 1);

		__util$faol (msgrec->text, &bodylen, &bodydsc, prm, n);
		if ( (int) bodylen > (UTIL$SZ_OUTBUF - olen - 1) )
			bodylen = (unsigned short) (UTIL$SZ_OUTBUF - olen - 1);
		memcpy (out + olen, body, (size_t) bodylen);
		olen += (int) bodylen;
		}
	else	{							/* No record - the default diagnostic */
		sev = $SEV(sts);
		olen += snprintf(out + olen, UTIL$SZ_OUTBUF - olen, s_defmsgfao, s_severity[sev], sts,
			$FAC(sts), $FAC(sts), (int) sev, (int) sev, $MSG(sts), $MSG(sts));
		}

	olen = $MIN(UTIL$SZ_OUTBUF - 1, olen);
	out[olen++] = '\n';

	s_util$logwrite (g_logoutput, out, olen);

	return	sts;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The FAO variant of __util$putmsgd(): as __util$putmsg_fao(), but with the
**	module/function/line location prefix used by the debug $PUTMSG_FAO macro.
**
**  FORMAL PARAMETERS:
**
**	sts		Condition/message code selecting the catalogue record
**	__mod,__fi,__li	Module, function and line of the call site
**	...		FAO parameters for the record text, each cast to fao_prm_t
**
**  RETURN VALUE:
**	sts
**--
*/
unsigned	__util$putmsgd_fao	(unsigned sts, const char *__mod, const char *__fi, unsigned __li, int prmcnt, ...)
{
va_list	arglist;
const char	lfmt [] = {"[%s:%u] "},
	mfmt [] = {"[%s\\%s:%u] "};
char	out [UTIL$SZ_OUTBUF + 8], body [UTIL$SZ_OUTBUF];
fao_desc_t	bodydsc = $FAO_DESC(body);
unsigned short	bodylen = 0;
int	olen, sev, n;
fao_prm_t	prm [16];
EMSG_RECORD *msgrec;

	olen = s_util$prefix (out);				/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */
	olen += __mod
		? snprintf (out + olen, UTIL$SZ_OUTBUF - olen, mfmt, __mod, __fi, __li)
		: snprintf (out + olen, UTIL$SZ_OUTBUF - olen, lfmt, __fi, __li);

	olen = $MIN(UTIL$SZ_OUTBUF - 1, olen);

	/*  The count is the caller's, see __util$putmsg_fao() above  */
	if ( prmcnt > (int) (sizeof(prm)/sizeof(prm[0])) )
		prmcnt = (int) (sizeof(prm)/sizeof(prm[0]));

	if ( prmcnt < 0 )
		prmcnt = 0;

	va_start (arglist, prmcnt);
	for ( n = 0; n < prmcnt; n++ )
		prm[n] = va_arg(arglist, fao_prm_t);
	va_end (arglist);

	if ( 1 & __util$getmsg(sts, &msgrec) )
		{
		/*  Prefix the caller's severity - %<S>- - as __util$putmsg_fao() does, mnemonic and all;
		**  see its own comment for the NULL-mnemonic fallback that keeps an old caller-built
		**  EMSG_RECORD (never set the field) on the plain %FAC-S- envelope.
		*/
		sev = $SEV(sts);
		olen += (msgrec->mnemonic && *msgrec->mnemonic)
			? snprintf(out + olen, UTIL$SZ_OUTBUF - olen, "%%%s-%c-%s, ",
				(msgrec->facname && *msgrec->facname) ? msgrec->facname : "NONAME", s_severity[sev], msgrec->mnemonic)
			: snprintf(out + olen, UTIL$SZ_OUTBUF - olen, "%%%s-%c-",
				(msgrec->facname && *msgrec->facname) ? msgrec->facname : "NONAME", s_severity[sev]);
		olen = $MIN(olen, UTIL$SZ_OUTBUF - 1);

		__util$faol (msgrec->text, &bodylen, &bodydsc, prm, n);
		if ( (int) bodylen > (UTIL$SZ_OUTBUF - olen - 1) )
			bodylen = (unsigned short) (UTIL$SZ_OUTBUF - olen - 1);
		memcpy (out + olen, body, (size_t) bodylen);
		olen += (int) bodylen;
		}
	else	{
		sev = $SEV(sts);
		olen += snprintf(out + olen, UTIL$SZ_OUTBUF - olen, s_defmsgfao, s_severity[sev], sts,
			$FAC(sts), $FAC(sts), (int) sev, (int) sev, $MSG(sts), $MSG(sts));
		}

	olen = $MIN(UTIL$SZ_OUTBUF - 1, olen);
	out[olen++] = '\n';

	s_util$logwrite (g_logoutput, out, olen);

	return	sts;
}






/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Format a record under a printf-style format and write it, with the
**	timestamp / severity / location prefix, to the current log destination.
**	This is the routine the $LOG macro expands to.
**
**  FORMAL PARAMETERS:
**
**	fac	Facility name for the prefix
**	sev	Record severity (STS$K_*)
**	fmt	printf-style format string
**	__mod	Call-site module name
**	__func	Call-site function name
**	__line	Call-site line number
**	...	Format arguments
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination (and the SYSLOG socket, if set).
**
**  RETURN VALUE:
**	The <sev> argument, so the call may be used in a return statement.
**--
*/
unsigned	__util$logd
			(
		const char *	fac,
		unsigned	sev,
		const char *	fmt,
		const char *	__mod,
		const char *	__func,
		unsigned	__line,
			...
			)

{
va_list arglist;
const char	__fmt [] = {"[%s\\%s:%u] %%%s-%c:  "};
char	out[UTIL$SZ_OUTBUF + 8];
unsigned olen, _sev = $SEV(sev);
#ifdef	__SYSLOG__
unsigned opcom = sev & STS$M_SYSLOG;
#endif


	sev &= ~STS$M_SYSLOG;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]-E-:" prefix
	*/
	olen = s_util$prefix (out);				/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */
	olen += snprintf (out + olen, UTIL$SZ_OUTBUF - olen, __fmt, __mod, __func, __line, fac, s_severity[_sev]);

	va_start (arglist, __line);
	olen += vsnprintf(out + olen, UTIL$SZ_OUTBUF - olen, fmt, arglist);
	va_end (arglist);

	olen = $MIN(olen, UTIL$SZ_OUTBUF - 1);

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	s_util$logwrite (g_logoutput, out, olen);

		/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, fac, "%.*s", olen, out);
	#endif


#ifdef	__SYSLOG__
#ifndef	WIN32
	va_start (arglist, __li);
	olen = vsnprintf(out, UTIL$SZ_OUTBUF, fmt, arglist);
	va_end (arglist);

	__util$syslog (LOG_USER, LOG_DEBUG, fac, out, olen);

//	syslog( LOG_PID | ((sev & 1) ? LOG_INFO : LOG_ERR), "%.*s", olen, out);
#endif
#endif	/* __SYSLOG__ */

	return	sev;
}









/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The FAO twin of __util$logd(): as __util$log_fao(), but with the
**	call-site [module\function:line] descriptor used by the debug
**	$LOG_FAO macro.
**
**  FORMAL PARAMETERS:
**
**	a_fac	Facility name for the envelope
**	a_sev	Record severity (STS$K_*)
**	a_ctl	FAO control string
**	a_mod	Call-site module name
**	a_func	Call-site function name
**	a_line	Call-site line number
**	a_prmcnt	Number of FAO parameters that follow (0..16)
**	...	FAO parameters, each already cast to fao_prm_t
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	The severity, as __util$logd() returns it.
**--
*/
unsigned	__util$logd_fao (
		const char	*a_fac,
		unsigned	a_sev,
		const char	*a_ctl,
		const char	*a_mod,
		const char	*a_func,
		unsigned	a_line,
		int	a_prmcnt,
			...
		)
{
va_list	l_arglist;
const char l_fmt [] = "[%s\\%s:%u] %%%s-%c:  ";
char	l_out [UTIL$SZ_OUTBUF + 8], l_body [UTIL$SZ_OUTBUF];
fao_desc_t	l_bodydsc = $FAO_DESC(l_body);
unsigned short	l_bodylen = 0;
unsigned l_olen, l_isev = $SEV(a_sev);
int	l_n;
fao_prm_t	l_prm [16];

	a_sev &= ~STS$M_SYSLOG;		/* as __util$logd() does	*/

	/* "DD-MM-YYYY HH:MM:SS.msc <tid> " + [mod\func:line] + %<fac>-<s>: */
	l_olen = s_util$prefix (l_out);
	l_olen += snprintf (l_out + l_olen, UTIL$SZ_OUTBUF - l_olen, l_fmt, a_mod, a_func, a_line, a_fac, s_severity[l_isev]);
	l_olen = $MIN(l_olen, UTIL$SZ_OUTBUF - 1);

	/* The FAO parameters: the count is the caller's, see __util$putmsg_fao() */
	if ( a_prmcnt > (int) (sizeof(l_prm)/sizeof(l_prm[0])) )
		a_prmcnt = (int) (sizeof(l_prm)/sizeof(l_prm[0]));

	if ( a_prmcnt < 0 )
		a_prmcnt = 0;

	va_start (l_arglist, a_prmcnt);
	for ( l_n = 0; l_n < a_prmcnt; l_n++ )
		l_prm[l_n] = va_arg(l_arglist, fao_prm_t);
	va_end (l_arglist);

	__util$faol (a_ctl, &l_bodylen, &l_bodydsc, l_prm, l_n);
	if ( (int) l_bodylen > (int) (UTIL$SZ_OUTBUF - l_olen - 1) )
		l_bodylen = (unsigned short) (UTIL$SZ_OUTBUF - l_olen - 1);
	memcpy (l_out + l_olen, l_body, (size_t) l_bodylen);
	l_olen += l_bodylen;

	l_out[l_olen++] = '\n';

	s_util$logwrite (g_logoutput, l_out, l_olen);

	/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, a_mod, "%.*s", l_olen, l_out);
	#endif

	return	a_sev;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Format a record in RFC-3164 syslog form and send it as a UDP datagram to
**	the SYSLOG host configured by __util$deflog(). A no-op if no host is set.
**
**  FORMAL PARAMETERS:
**
**	fac	Syslog facility code
**	sev	Syslog severity code
**	tag	Message tag (program/facility name)
**	msg	Message text
**	msglen	Length of <msg> in octets
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Sends a datagram on the module-static SYSLOG socket.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on success; a condition code otherwise.
**--
*/

unsigned	__util$syslog
		(
		int		fac,
		int		sev,
		const char *	tag,
		const char *	msg,
		int		msglen
		)
{
static int	sd = -1;
char	buf[128];

#ifdef	_WIN32

WSAMSG msg_desc = { 0 };
WSABUF   bufv[] = {{0, buf, }, {msglen, msg}};

#else
struct	iovec bufv[] = {{buf, 0}, {(void *) msg, msglen}};
struct	msghdr  msg_desc = {0};
#endif


	if ( sd == -1 )
		{
		/*
		inet_aton("172.28.4.112", &slogsock.sin_addr);
		slogsock.sin_port = htons(514);
		slogsock.sin_family = AF_INET;
		*/
#ifdef	_WIN32
		WSADATA wsa_data;
		int status;

		if ( status = WSAStartup(MAKEWORD(2,2), &wsa_data) )
			{
			perror("WSAstartup");
			abort();
			}

#endif
		if ( 0 > (sd = socket(AF_INET, SOCK_DGRAM, 0)) )
			{
			perror("socket");
			return	STS$K_ERROR;
			}
		}

	/* Prepare the msghdr structure */
#ifdef	_WIN32
	msg_desc.name = (LPSOCKADDR) &slogsock;
	msg_desc.namelen = sizeof(slogsock);

	msg_desc.lpBuffers = (void *) &bufv;
	msg_desc.dwBufferCount = sizeof(bufv)/sizeof(bufv[0]);

	/* Form <priority + severity> part, add the tag field */
	bufv[0].len = snprintf(buf, sizeof(buf), "<%d> %s: ", fac  + sev, tag);

	if ( 0 >  WSASendMsg (sd, &msg_desc, 0, 0, 0, 0) )
		{
		perror("sendto");
		return	STS$K_ERROR;
		}


#else
	msg_desc.msg_name = &slogsock;
	msg_desc.msg_namelen = sizeof(slogsock);

	msg_desc.msg_iov = (void *) &bufv;
	msg_desc.msg_iovlen = sizeof(bufv)/sizeof(bufv[0]);

	/* Form <priority + severity> part, add the tag field */
	bufv[0].iov_len = snprintf(buf, sizeof(buf), "<%d> %16s: ", fac  + sev, tag);
	if ( 0 >  sendmsg (sd, &msg_desc, 0) )
		{
		perror("sendto");
		return	STS$K_ERROR;
		}

#endif


	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write the current value of every descriptor in an option table to the
**	log destination. Password descriptors (OPTS$K_PWD) are shown masked.
**
**  FORMAL PARAMETERS:
**
**	opts	Option table: address of an OPTS array terminated by OPTS_NULL
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	STS$K_SUCCESS.
**--
*/
int	__util$showparams	(
			const OPTS *	opts
			)
{
OPTS	*optp;

	for (optp = (OPTS *) opts; $ASCLEN(&optp->name); optp++)
		{
		switch ( optp->type )
			{
			case	OPTS$K_PORT:
				$LOG(STS$K_INFO, "%.*s = %d (0x%04.4x)", $ASC(&optp->name),
					 *((unsigned short *) optp->ptr), *((unsigned short *) optp->ptr));
				break;

			case	OPTS$K_IP4:
				$LOG(STS$K_INFO, "%.*s = %s", $ASC(&optp->name), inet_ntoa( *((struct in_addr *) optp->ptr)) );
				break;

			case	OPTS$K_OPT:
				$LOG(STS$K_INFO, "%.*s = %s", $ASC(&optp->name), * ((int *)optp->ptr) ? "ON" : "OFF");
				break;

			 case	OPTS$K_INT:
				switch ( optp->sz )
					{
					case (sizeof (unsigned long long)):
						$LOG(STS$K_INFO, "%.*s = %llu (%#llx)", $ASC(&optp->name), *((unsigned long long *) optp->ptr),
							*((unsigned long long *) optp->ptr));
						break;

					case (sizeof (unsigned short)):
						$LOG(STS$K_INFO, "%.*s = %d (%#x)", $ASC(&optp->name), *((unsigned short *) optp->ptr),
							*((unsigned short *) optp->ptr));
						break;

					default:
						$LOG(STS$K_INFO, "%.*s = %d (%#x)", $ASC(&optp->name), *((int *) optp->ptr), *((int *) optp->ptr));
						break;
					}
				break;

			case	OPTS$K_STR:
				$LOG(STS$K_INFO, "%.*s[0:%u] ='%.*s'", $ASC(&optp->name), ((ASC *)optp->ptr)->len, $ASC(optp->ptr));
				break;

			case	OPTS$K_PWD:
				$LOG(STS$K_INFO, "%.*s[0:%u] ='<password>'", $ASC(&optp->name), ((ASC *)optp->ptr)->len, $ASC(optp->ptr));
				break;

			}
		}

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Fill a table of typed option descriptors from a text configuration
**	file. Each line has the form -name=value; text after a '!' is a comment.
**	The same OPTS table may be shared with __util$getparams().
**
**  FORMAL PARAMETERS:
**
**	fconf	Zero-terminated path of the configuration file
**	opts	OPTS table terminated by OPTS_NULL
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the named file; updates the variables named by the descriptors.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on success; STS$K_ERROR on a bad file name, an
**	unreadable file or an invalid value.
**--
*/
int	__util$readconfig	(
		const	char *	fconf,
			OPTS *	opts
			)
{
int	i, argslen;
FILE	*finp = stdin;
const char novalp [ 32 ] = {0};
char	*argp, *valp = (char *) novalp, buf[256];
OPTS	*optp, *optp2;

	/*
	 * Is the configuration file has been specified ?
	 */
	if ( !fconf || !(*fconf) )
		return	$LOG(STS$K_ERROR, "No configuration file is provided, check input");

	if ( !(finp = fopen(fconf, "r")) )
		return	$LOG(STS$K_ERROR, "Error open file '%s', errno = %d", fconf, errno);

	/*
	* We expect to see opts in form: -<options_name>=<options_value>
	*/
	for ( i = 1; fgets(buf, sizeof(buf), finp); i++ )
		{
		if ( !__util$uncomment(buf, (int) strlen(buf), '!') )
			continue;

		if ( !__util$trim(buf, (int) strlen(buf)) )
			continue;

		if ( (*(argp = buf) != '-') && (*(argp = buf) != '/') )
			{
			$LOG(STS$K_ERROR, "%s :%d : Option must be started with a '-' or '/', skip : '%s'", fconf, i, buf);
			continue;
			}

		if ( (valp = strchr(++argp, '=')) )
			{
			argslen = (int) (valp - argp);
			valp++;
			}
		else	{
			argslen = (int) strlen(argp);
			valp = (char *) novalp;
			}


		/*
		* Compare a given from command line option name against options list,
		* we expect that options list is terminated by zero-length entry.
		*
		* We allow an shortened keywords but it can be a reason of conflicts.
		*/
		for (optp2 = NULL, optp = opts; $ASCLEN(&optp->name); optp++)
			{
			/* Input longer than the option name can not be an abbreviation of it - skip to avoid false matches
			 * (e.g. "NSERVERXYZ" must not match "NSERVER")
			 */
			if ( (unsigned) argslen > $ASCLEN(&optp->name) )
				continue;

#if _WIN32
			if ( !(_strnicmp (argp, $ASCPTR(&optp->name), $MIN(argslen, $ASCLEN(&optp->name)))) )
#else
			if ( !(strncasecmp (argp, $ASCPTR(&optp->name), $MIN(argslen, $ASCLEN(&optp->name)))) )
#endif
				{
				if ( argslen == $ASCLEN(&optp->name) )	/* Full matching */
					{
					optp2 = optp;
					break;
					}

				if ( !optp2 )	/* If it's first hit - save option */
					{
					optp2 = optp;
					continue;
					}


				/*
				 * Is the new hit is longest than old ?
				*/
				if ( $ASCLEN(&optp2->name) < $ASCLEN(&optp->name) )
					optp2 = optp;
				}
			}

		if ( !optp2 || !$ASCLEN(&optp2->name) )
			{
			$LOG(STS$K_WARN, "%s :%d : Skip unrecognized or unused option", fconf, i);
			continue;
			}


		optp = optp2 ? optp2 : optp;

		if ( valp && *valp)
			__util$trim (valp, (int) strlen(valp));

		switch ( optp->type )
			{
			case	OPTS$K_PORT:
				{
				char *l_endptr = NULL;

				*((unsigned short *) optp->ptr) = (unsigned short) strtoul(valp, &l_endptr, 0);
				}
				break;

			case	OPTS$K_IP4:
#ifdef WIN32
				if ( ! ((*(long *) optp->ptr) =  inet_addr (valp)) )
#else
				if ( !inet_aton(valp, (struct in_addr *) optp->ptr) )
#endif
					$LOG(STS$K_ERROR, "%s :%d : Error conversion '%s' to IP4 address", fconf, i, valp);
				break;

			case	OPTS$K_OPT:
				* ((int *)optp->ptr) = 1;
				break;

			case	OPTS$K_INT:
				{
				char *endptr = NULL;


				switch ( optp->sz )
					{
					case (sizeof (unsigned long long)):
						* ((unsigned long long *) optp->ptr) =  strtoull(valp, &endptr, 0);
						break;

					default:
						* ((int *) optp->ptr) =  strtoul(valp, &endptr, 0);
						break;

					}
				}
				break;

			case	OPTS$K_PWD:
			case	OPTS$K_STR:
				((ASC *)optp->ptr)->len  = (unsigned char) $MIN(strnlen(valp, optp->sz), ASC$K_SZ);
				memcpy( ((ASC *)optp->ptr)->sts, valp, ((ASC *)optp->ptr)->len);
				((ASC *)optp->ptr)->sts[((ASC *)optp->ptr)->len] = '\0';
				break;

			default:
				$LOG(STS$K_ERROR, "%s :%d : Unrecognized option's '%.*s' internal type : 0x%X", fconf, i, $ASC(&optp->name), optp->type);
			}
		}

	if ( finp && (finp != stdin) )
		fclose(finp);


	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Parse the program argument vector against a table of typed option
**	descriptors. Accepts -name=value, /name=value and --name=value; names
**	may be abbreviated to any unambiguous prefix. An OPTS$K_CONF option
**	triggers an immediate read of the named file via __util$readconfig().
**
**  FORMAL PARAMETERS:
**
**	argc	Argument count, as passed to main()
**	argv	Argument vector
**	opts	OPTS table terminated by OPTS_NULL
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Updates the variables named by the descriptors.
**
**  RETURN VALUE:
**	STS$K_SUCCESS if all options parsed; STS$K_ERROR on a bad value or a
**	missing mandatory option.
**--
*/
int	__util$getparams	(
			int	argc,
			char *	argv[],
		const OPTS *	opts
			)
{
int	i;
size_t	argslen;
const char novalp [ 32 ] = {0};
char	*argp, *valp = (char *) novalp;
OPTS	*optp;


	/*
	* We expect to see opts in form:
	*	-<options_name>=<options_value>
	*  or
	*	/<options_name>=<options_value>
	*/
	for ( i = 1; i < argc; i++ )
		{
		/* Skip zero length strings	*/
		if ( !strlen (argv[i]) )
			continue;

		/* Check option designator	*/
		if ( (*(argp = argv[i]) != '-') && (*(argp = argv[i]) != '/') )
			{
			$LOG(STS$K_ERROR, "%d: Option must be started with '-' or '/', skip : '%s'", i, argv[i]);
			continue;
			}

		if ( *(argp + 1) == '-' )
			argp++;


		if ( (valp = strchr(++argp, '=')) )
			{
			argslen = valp - argp;
			valp++;
			}
		else	{
			argslen = strlen(argp);
			valp = (char *) novalp;
			}

		/*
		* Compare a given from command line option name against options list,
		* we expect that options list is terminated by zero-length entry.
		*
		* We allow an shortened keywords but it can be a reason of conflicts.
		*/
		for (optp = (OPTS *) opts; $ASCLEN(&optp->name); optp++)
			{
			if ( argslen != $ASCLEN(&optp->name) )
				continue;

#if _WIN32
			if ( !(_strnicmp (argp, $ASCPTR(&optp->name), $MIN(argslen, $ASCLEN(&optp->name)))) )
#else
			if ( !(strncasecmp (argp, $ASCPTR(&optp->name), $MIN(argslen, $ASCLEN(&optp->name)))) )
#endif
				break;
			}

		if ( !optp || !$ASCLEN(&optp->name) )
			{
			$LOG(STS$K_ERROR, "%d : Skip unrecognized option: '%s'", i, argp);
			continue;
			}

		if ( valp && *valp)
			__util$trim (valp, (int) strlen(valp));

		switch ( optp->type )
			{
			case	OPTS$K_CONF:
				if ( optp->ptr )
					{
					/* Store name of configuration file	*/
					((ASC *)optp->ptr)->len  = (unsigned char) strnlen(valp, sizeof(((ASC *)optp->ptr)->sts));
					memcpy( ((ASC *)optp->ptr)->sts, valp, ((ASC *)optp->ptr)->len);
					((ASC *)optp->ptr)->sts[((ASC *)optp->ptr)->len] = '\0';
					}

				if ( !(1 & __util$readconfig (valp, (OPTS *) opts)) )
					return	STS$K_ERROR;

				break;

			case	OPTS$K_PORT:
				{
				char *l_endptr = NULL;

				*((unsigned short *) optp->ptr) = (unsigned short) strtoul(valp, &l_endptr, 0);
				}
				break;

			case	OPTS$K_IP4:
#ifdef WIN32
				if ( ! ((*(long *) optp->ptr) =  inet_addr (valp)) )
#else
				if ( !inet_aton(valp, (struct in_addr *) optp->ptr) )
#endif
					$LOG(STS$K_ERROR, "%d : Error conversion '%s' to IP4 address", i, valp);
				break;

			case	OPTS$K_OPT:
				* ((int *)optp->ptr) = 1;
				break;

			case	OPTS$K_INT:
				{
				char *endptr = NULL;

				switch ( optp->sz )
					{
					case (sizeof (unsigned long long)):
						* ((unsigned long long *) optp->ptr) =  strtoull(valp, &endptr, 0);
						break;

					default:
						* ((int *) optp->ptr) =  strtoul(valp, &endptr, 0);
						break;

					}
				}
				break;

			case	OPTS$K_PWD:
			case	OPTS$K_STR:
				((ASC *)optp->ptr)->len  = (unsigned char) $MIN(strnlen(valp, optp->sz), ASC$K_SZ);
				memcpy( ((ASC *)optp->ptr)->sts, valp, ((ASC *)optp->ptr)->len);
				((ASC *)optp->ptr)->sts[((ASC *)optp->ptr)->len] = '\0';
				break;

			default:
				$LOG(STS$K_ERROR, "%d : Unrecognized option's '%.*s' internal type : 0x%X", i, $ASC(&optp->name), optp->type);
			}
		}

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write a formatted hexadecimal/ASCII dump of a memory region to the log
**	destination, laid out sixteen octets per line as 'offset: hex ... ascii'.
**	Normally invoked through the $DUMPHEX macro, which supplies the call site.
**
**  FORMAL PARAMETERS:
**
**	a__fi	Call-site function name (from $DUMPHEX)
**	a__li	Call-site line number (from $DUMPHEX)
**	a_src	Address of the region to dump
**	a_srclen	Number of octets to dump
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	NONE.
**--
*/

void	__util$dumphex	(
		const char *	a__fi,
		unsigned	a__li,
		const void *	a_src,
		unsigned short	a_srclen
			)
{
const char	l_fmt [] = {"[%s:%u] Dump of %u octets follows:"};
#define		UTILS$SZ_HEXWIDTH	80
char	l_out[256];
unsigned char *l_src = (unsigned char *) a_src, l_low, l_high;
unsigned l_olen = 0, i, j;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]" prefix
	*/
	l_olen = s_util$prefix (l_out);			/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */
	l_olen += snprintf (l_out + l_olen, sizeof(l_out) - 1 - l_olen, l_fmt, a__fi, a__li, a_srclen);


	/* Add <LF> at end of record*/
	l_olen = $MIN(sizeof(l_out) - 1, l_olen);
	l_out[l_olen++] = '\n';

	s_util$logwrite (g_logoutput, l_out, l_olen );

	memset(l_out, ' ', UTILS$SZ_HEXWIDTH);

	/*
	** Format variable part of string line
	*/
	for (i = 0; i < ((a_srclen / 16));  i++)
		{
		l_olen = snprintf(l_out, UTILS$SZ_HEXWIDTH, "\t+%04x:  ", i * 16);
		l_olen = $MIN(UTILS$SZ_HEXWIDTH - 1, l_olen);

		memset(l_out + l_olen, ' ', UTILS$SZ_HEXWIDTH - l_olen);

		for (j = 0; j < 16; j++, l_src++)
			{
			l_high = (*l_src) >> 4;
			l_low = (*l_src) & 0x0f;

			l_out[l_olen + j * 3] = l_high + ((l_high < 10) ? '0' : 'a' - 10);
			l_out[l_olen + j * 3 + 1] = l_low + ((l_low < 10) ? '0' : 'a' - 10);

			l_out[l_olen + 16*3 + 2 + j] = isprint(*l_src) ? *l_src : '.';
			}

		/* Add <LF> at end of record*/
		l_out[UTILS$SZ_HEXWIDTH - 1] = '\n';

		/* Write to file and flush buffer depending on severity level */
		s_util$logwrite (g_logoutput, l_out, UTILS$SZ_HEXWIDTH );
		}

	if ( a_srclen % 16 )
		{
		l_olen = snprintf(l_out, UTILS$SZ_HEXWIDTH, "\t+%04x:  ", i * 16);
		l_olen = $MIN(UTILS$SZ_HEXWIDTH - 1, l_olen);

		memset(l_out + l_olen, ' ', UTILS$SZ_HEXWIDTH - l_olen);

		for (j = 0; j < a_srclen % 16; j++, l_src++)
			{
			l_high = (*l_src) >> 4;
			l_low = (*l_src) & 0x0f;

			l_out[l_olen + j * 3] = l_high + ((l_high < 10) ? '0' : 'a' - 10);
			l_out[l_olen + j * 3 + 1] = l_low + ((l_low < 10) ? '0' : 'a' - 10);

			l_out[l_olen + 16*3 + 2 + j] = isprint(*l_src) ? *l_src : '.';
			}

		/* Add <LF> at end of record*/
		l_out[UTILS$SZ_HEXWIDTH - 1] = '\n';

		/* Write to file and flush buffer depending on severity level */
		s_util$logwrite (g_logoutput, l_out, UTILS$SZ_HEXWIDTH );
		}
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Format up to sixteen octets as a single hex-and-ASCII dump line of the
**	form 'XX XX ... XX  ....ascii....'. Unused columns are space-padded and
**	non-printable octets are shown as a dot. Does not read past <a_src>.
**
**  FORMAL PARAMETERS:
**
**	a_src	Address of the octets to format
**	a_srclen	Number of octets; values above 16 are clamped to 16
**	a_out	Destination buffer
**	a_outsz	Size of <a_out>; must be at least 67 bytes or the routine returns 0
**	a_outlen	Receives the number of bytes written
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE.
**
**  RETURN VALUE:
**	Number of bytes written; 0 indicates <a_outsz> was too small.
**--
*/
int	__util$faohex	(
	const void *	a_src,
	unsigned short	a_srclen,
		char *	a_out,
	unsigned short	a_outsz,
	unsigned short	*a_outlen
			)
{
unsigned char *l_src = (unsigned char *) a_src, l_low, l_high;
unsigned l_j;

	/* Required room: 16*3 (hex) + 2 (gap) + 16 (ascii) + 1 (NUL) = 67 octets */
	if ( a_outsz < (16*3 + 2 + 16 + 1) )
		return	(*a_outlen = 0);

	memset(a_out, ' ', a_outsz);
	a_srclen = $MIN(16, a_srclen);

	/* Iterate only over the actually supplied octets - do not read past <a_src> */
	for (l_j = 0; l_j < a_srclen; l_j++, l_src++)
		{
		l_high = (*l_src) >> 4;
		l_low = (*l_src) & 0x0f;

		a_out[l_j * 3] = l_high + ((l_high < 10) ? '0' : 'a' - 10);
		a_out[l_j * 3 + 1] = l_low + ((l_low < 10) ? '0' : 'a' - 10);

		a_out[16*3 + 2 + l_j] = isprint(*l_src) ? *l_src : '.';
		}

	a_out[16*3 + 2 + 16] = '\0';

	return (*a_outlen = (16*3 + 2 + 16));
}









/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Emit a diagnostic trace record carrying the call-site module, function
**	and line. Reached through the $TRACE macro (and $IFTRACE, which guards
**	the record on a condition). Compiles to nothing under -DNDEBUG.
**
**  FORMAL PARAMETERS:
**
**	cond	Emit the record only when non-zero (from $IFTRACE)
**	fmt	A format string
**	__mod	Module name string
**	__fi	Function name string
**	__li	Line number in the source file
**	...	Format string arguments
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	NONE.
**--
*/
static const	char spaces[64] = {"                                                                "};

void	__util$trace	(
			int	cond,
		const char *	fmt,
		const char *	__mod,
		const char *	__fi,
		unsigned	__li,
			...
			)
{
va_list arglist;

const char	lfmt [] = {"[%s:%u] "},
	mfmt [] = {"[%s\\\\%s:%u] "};
char	out[1024];
int	olen, len;

	if ( !cond )
		return;


	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]" prefix
	*/
	olen = s_util$prefix (out);				/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */
	olen += __mod
		? snprintf (out + olen, sizeof(out) - olen, mfmt, __mod, __fi, __li)
		: snprintf (out + olen, sizeof(out) - olen, lfmt, __fi, __li);

	if ( 0 < (len = (72 - olen)) )
		{
		memcpy(out + olen, spaces, len);
		olen += len;
		}

	/*
	** Format variable part of string line
	*/
	va_start (arglist, __li);
	olen += vsnprintf(out + olen, sizeof(out) - olen, fmt, arglist);
	va_end (arglist);

	olen = $MIN(olen, sizeof(out) - 1);

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer */
	s_util$logwrite (g_logoutput, out, olen );



	/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, __mod, "%.*s", olen, out);
	#endif

#ifdef	__SYSLOG__
#ifndef	WIN32
	va_start (arglist, __li);
	olen = vsnprintf(out, sizeof(out), fmt, arglist);
	va_end (arglist);

	__util$syslog (LOG_USER, LOG_DEBUG, __mod, out, olen);

//	syslog( LOG_PID | ((sev & 1) ? LOG_INFO : LOG_ERR), "%.*s", olen, out);
#endif
#endif	/* __SYSLOG__ */

}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Like __util$logd() but without an explicit call-site descriptor: format
**	a record and write it to the current log destination.
**
**  FORMAL PARAMETERS:
**
**	fac	Facility name
**	sev	Record severity
**	fmt	printf-style format string
**	...	Format arguments
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	The <sev> argument.
**--
*/
unsigned	__util$log
			(
		const char *	fac,
		unsigned	sev,
		const char *	fmt,
			...
			)

{
va_list arglist;
const char lfmt [] = "%%%s-%C: ";
char	out[UTIL$SZ_OUTBUF + 8];
unsigned olen, _sev = $SEV(sev), opcom = sev & STS$M_SYSLOG;

	/*
	** Some sanity check
	*/
	/* BATCH: STS$K_FATAL is 4 and was clamped to STS$K_UNDEF, which is odd and so reads as success */
	if ( !($ISINRANGE(sev, STS$K_WARN, STS$K_FATAL)) )
		sev = STS$K_UNDEF;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]<FAC>-E:" prefix
	*/
	olen = s_util$prefix (out);				/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */
	olen += snprintf (out + olen, UTIL$SZ_OUTBUF - olen, lfmt, fac, s_severity[_sev]);

	va_start (arglist, fmt);
	olen += vsnprintf(out + olen, UTIL$SZ_OUTBUF - olen, fmt, arglist);
	va_end (arglist);

	olen = $MIN(olen, UTIL$SZ_OUTBUF - 1);			/* Not sizeof(out): the padding is uninitialized	*/

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	s_util$logwrite (g_logoutput, out, olen);

	/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, fac, "%.*s", olen, out);
	#endif


#ifndef	WIN32
	if ( opcom )
		{
		va_start (arglist, fmt);
		olen = vsnprintf(out, UTIL$SZ_OUTBUF, fmt, arglist);
		va_end (arglist);

		syslog( LOG_PID | ((sev & 1) ? LOG_INFO : LOG_ERR), "%.*s", olen, out);
		}
#endif

	return	sev;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The FAO twin of __util$log(): format a record under an FAO control
**	string (via __util$faol(), so !UL/!UQ/!AZ/!AS/!XL/!%I/... are at hand)
**	and write it, with the usual timestamp / %<fac>-<S>: envelope, to the
**	current log destination. Reached through the $LOG_FAO macro, which
**	casts every parameter to fao_prm_t and derives the count for the
**	caller - exactly the $PUTMSG_FAO ergonomics.
**
**  FORMAL PARAMETERS:
**
**	a_fac	Facility name for the envelope
**	a_sev	Record severity (STS$K_*, optionally | STS$M_SYSLOG)
**	a_ctl	FAO control string
**	a_prmcnt	Number of FAO parameters that follow (0..16)
**	...	FAO parameters, each already cast to fao_prm_t
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination (and syslog on STS$M_SYSLOG).
**
**  RETURN VALUE:
**	The severity, as __util$log() returns it.
**--
*/
unsigned	__util$log_fao (
		const char	*a_fac,
		unsigned	a_sev,
		const char	*a_ctl,
		int	a_prmcnt,
			...
		)
{
va_list	l_arglist;
const char l_fmt [] = "%%%s-%C: ";
char	l_out [UTIL$SZ_OUTBUF + 8], l_body [UTIL$SZ_OUTBUF];
fao_desc_t	l_bodydsc = $FAO_DESC(l_body);
unsigned short	l_bodylen = 0;
unsigned l_olen, l_isev = $SEV(a_sev), l_opcom = a_sev & STS$M_SYSLOG;
int	l_n;
fao_prm_t	l_prm [16];

	/* The same sanity clamp as __util$log() applies	*/
	if ( !($ISINRANGE(a_sev, STS$K_WARN, STS$K_FATAL)) )
		a_sev = STS$K_UNDEF;

	/* "DD-MM-YYYY HH:MM:SS.msc <tid> " + the %<fac>-<S>: envelope	*/
	l_olen = s_util$prefix (l_out);
	l_olen += snprintf (l_out + l_olen, UTIL$SZ_OUTBUF - l_olen, l_fmt, a_fac, s_severity[l_isev]);
	l_olen = $MIN(l_olen, UTIL$SZ_OUTBUF - 1);

	/* The FAO parameters: the count is the caller's, see __util$putmsg_fao() */
	if ( a_prmcnt > (int) (sizeof(l_prm)/sizeof(l_prm[0])) )
		a_prmcnt = (int) (sizeof(l_prm)/sizeof(l_prm[0]));

	if ( a_prmcnt < 0 )
		a_prmcnt = 0;

	va_start (l_arglist, a_prmcnt);
	for ( l_n = 0; l_n < a_prmcnt; l_n++ )
		l_prm[l_n] = va_arg(l_arglist, fao_prm_t);
	va_end (l_arglist);

	__util$faol (a_ctl, &l_bodylen, &l_bodydsc, l_prm, l_n);
	if ( (int) l_bodylen > (int) (UTIL$SZ_OUTBUF - l_olen - 1) )
		l_bodylen = (unsigned short) (UTIL$SZ_OUTBUF - l_olen - 1);
	memcpy (l_out + l_olen, l_body, (size_t) l_bodylen);
	l_olen += l_bodylen;

	l_out[l_olen++] = '\n';

	s_util$logwrite (g_logoutput, l_out, l_olen);

	/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, a_fac, "%.*s", l_olen, l_out);
	#endif

#ifndef	WIN32
	if ( l_opcom )		/* The body is formatted already - reuse it	*/
		syslog( LOG_PID | ((a_sev & 1) ? LOG_INFO : LOG_ERR), "%.*s", (int) l_bodylen, l_body);
#endif

	return	a_sev;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Format a log record exactly as $LOG would, but deliver it into a caller-
**	supplied buffer instead of the log destination - handy when the text is
**	needed as a string (a protocol field, a UI element, and so on).
**
**  FORMAL PARAMETERS:
**
**	out	Address of the output buffer
**	outsz	Size of <out> in bytes
**	outlen	Receives the number of bytes written
**	fac	Facility name
**	sev	Record severity
**	fmt	printf-style format string
**	...	Format arguments
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE.
**
**  RETURN VALUE:
**	The <sev> argument.
**--
*/
unsigned	__util$log2buf
			(
		void	*	out,
		int		outsz,
		int	*	outlen,
		const char *	fac,
		unsigned	sev,
		const char *	fmt,
			...
			)

{
va_list arglist;
const char lfmt [] = "%%%s-%C:";
char	*__outbuf = (char *) out;
unsigned	_sev = sev;
char	l_pfx [48];
int	l_pl;

	/*
	** Some sanity check
	*/
	/* BATCH: STS$K_FATAL is 4 and was clamped to STS$K_UNDEF, which is odd and so reads as success */
	if ( !($ISINRANGE(sev, STS$K_WARN, STS$K_FATAL)) )
		sev = STS$K_UNDEF;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]<FAC>-E:" prefix
	*/
	l_pl = s_util$prefix (l_pfx);			/* "DD-MM-YYYY HH:MM:SS.msc <tid> " */

	if ( outsz < 1 )				/* no usable room in the caller buffer	*/
		{
		*outlen = 0;
		return	_sev;
		}

	*outlen = $MIN(l_pl, outsz - 1);		/* the prefix, clamped to the caller buffer */
	memcpy (__outbuf, l_pfx, (size_t) *outlen);
	*outlen += snprintf (__outbuf + *outlen, outsz - *outlen, lfmt, fac, s_severity[sev]);

	/* snprintf() returns the length it WOULD have written; clamp before reusing
	** it as an offset, so the append below never gets a negative (huge) size when
	** the prefix alone already fills the buffer.
	*/
	*outlen = $MIN(*outlen, outsz - 1);

	va_start (arglist, fmt);
	*outlen += vsnprintf(__outbuf + *outlen, outsz - *outlen, fmt, arglist);
	va_end (arglist);

	/* outsz-1, not outsz: the explicit terminator below writes __outbuf[*outlen],
	** so *outlen == outsz would land one byte past the caller's buffer.
	*/
	*outlen = $MIN(*outlen, outsz - 1);

	/* Add NIL at end of record*/
	__outbuf[*outlen] = '\0';

	return	_sev;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Redirect subsequent log output to a file and, optionally, forward each
**	record as a UDP datagram to a SYSLOG host. Called once at start-up,
**	before the first $LOG.
**
**  FORMAL PARAMETERS:
**
**	logfile	Zero-terminated log file path to append to, or NULL for stdout
**	sloghost	Zero-terminated IPv4 address of a SYSLOG server, or NULL
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Sets the global g_logoutput descriptor (and the SYSLOG socket).
**
**  RETURN VALUE:
**	STS$K_SUCCESS on success; the file could not be opened otherwise.
**--
*/
int	__util$deflog	(
		const char *	logfile,
		const char *	sloghost
				)
{
#ifdef	_WIN32
int	mode = _S_IREAD | _S_IWRITE;
#else
const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#endif

int	fd = -1;

	/* Is there SYSLOG Host IP ? */
	if ( sloghost )
		{
		inet_pton(AF_INET, sloghost, &slogsock.sin_addr);
		slogsock.sin_port = htons(514);
		slogsock.sin_family = AF_INET;
		}

	if ( !logfile )
		return	STS$K_SUCCESS;

	if ( !*logfile)
		return	STS$K_SUCCESS;

	if ( 0 > (fd = open(logfile, O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC, mode)) )
		return	$LOG(STS$K_ERROR, "Error opening log file '%s', errno = %d.", logfile, errno);

	lseek(fd, 0L, SEEK_END);

	/* Called again: the file it opened the time before is not needed any more;
	 * a descriptor given to __util$setlogfd() is the caller's, not closed.
	 */
	if ( s_logpathlen && (g_logoutput > STDERR_FILENO) && (g_logoutput != fd) )
		close (g_logoutput);

	g_logoutput = fd;

	/* Kept for __util$rotatelogfile(); a name too long for it disables rotation	*/
	s_logpathlen = strnlen(logfile, sizeof(s_logpath));

	if ( s_logpathlen < sizeof(s_logpath) )
		memcpy(s_logpath, logfile, s_logpathlen + 1);
	else	s_logpathlen = 0;

	dup2(fd, STDERR_FILENO);
	dup2(fd, STDOUT_FILENO);


	return $LOG(STS$K_SUCCESS, "Log file '%s' has been opened.", logfile);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Point the logging at an already-open descriptor, without opening a file
**	or touching the standard streams.  The companion to __util$deflog(),
**	which takes a file NAME and seizes both stdout and stderr with dup2() -
**	right for a daemon, wrong for a utility whose standard output carries
**	the data it was run to produce.  Such a utility calls
**	__util$setlogfd(STDERR_FILENO) once, keeping its output clean while its
**	$LOG / $TRACE records go to the standard error.
**
**  FORMAL PARAMETERS:
**
**	a_fd	An already-open descriptor to receive the log records
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Sets the global g_logoutput descriptor.
**
**  RETURN VALUE:
**	STS$K_SUCCESS; STS$K_ERROR if a_fd is negative.
**--
*/
int	__util$setlogfd	(
		int	a_fd
			)
{
	if ( a_fd < 0 )
		return	STS$K_ERROR;

	g_logoutput = a_fd;
	s_logpathlen = 0;						/* Not a file of ours: never rotated	*/

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Truncate and rewind the current log file once it grows past a limit, so
**	that a long-running process cannot fill the disk. A no-op when the log
**	goes to standard output or the limit is zero.
**
**  FORMAL PARAMETERS:
**
**	limit	Maximum log-file size in bytes before it is rewound
**
**  IMPLICIT INPUTS/OUTPUTS:
**	May truncate the file behind the global g_logoutput descriptor.
**
**  RETURN VALUE:
**	STS$K_SUCCESS.
**--
*/
int	__util$rewindlogfile	(
			size_t	limit
				)
{
int	status;

	if ( (g_logoutput == STDOUT_FILENO) || !limit )
		return	STS$K_SUCCESS;

	if ( 0 > (status = lseek (g_logoutput, 0, SEEK_CUR)) )
		return	$LOG(STS$K_ERROR, "lseek() -> %d", errno);

	if ( status < (int) limit )
		return	STS$K_SUCCESS;

	if ( 0 > (status = lseek(g_logoutput, 0L, SEEK_SET)) )
		$LOG(STS$K_ERROR, "fseek() -> %d", errno);

#ifndef	WIN32
	if ( 0  > (ftruncate(g_logoutput, status )) )
		$LOG(STS$K_ERROR, "ftruncate(%d, %d) -> %d", g_logoutput, status, errno);
#endif


	return	STS$K_SUCCESS;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Rotate the log file once it grows past a limit. Called periodically by a
**	long-running process (once in a few seconds is plenty), it checks the
**	size of the file __util$deflog() opened and, past the limit, renames
**	<file>.<keep-1> to <file>.<keep> ... <file> to <file>.1 - the oldest is
**	replaced - then opens a fresh <file> on the very descriptor the logging
**	writes to, and on the standard output and error with it. Nothing is
**	copied: a rename is a metadata update, so a slow disk is not stalled.
**	A record written by another thread meanwhile lands either at the end of
**	the old file or at the start of the new one, never torn.
**
**	keep = 0 truncates the file in place instead: the old records are lost,
**	as with __util$rewindlogfile(), but unlike it the size is checked on
**	every call, not once.
**
**	A no-op when the log goes to the standard output (no __util$deflog()
**	with a file), the limit is zero or the file is still short of it.
**
**  FORMAL PARAMETERS:
**
**	a_limit	Size in bytes past which the file is rotated, 0 - never
**	a_keep	How many old files to keep: <file>.1 ... <file>.<keep>, 0...99
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Inputs:	the file name remembered by __util$deflog()
**	Outputs: g_logoutput, STDOUT_FILENO and STDERR_FILENO point at the new file
**
**  RETURN VALUE:
**	STS$K_SUCCESS	rotated, or nothing to do yet
**	STS$K_ERROR	a_keep out of range, or a system service failed (the
**			errno is in the record logged); the logging goes on into
**			the file it was writing to
**--
*/
int	__util$rotatelogfile	(
		size_t		a_limit,
		unsigned	a_keep
			)
{
struct stat	l_st;
char		l_old [sizeof(s_logpath) + 8], l_new [sizeof(s_logpath) + 8];
int		l_fd, l_sts = STS$K_SUCCESS;
#ifdef	_WIN32
int		l_mode = _S_IREAD | _S_IWRITE;
#else
const mode_t	l_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#endif

	if ( a_keep > 99 )
		return	$LOG(STS$K_ERROR, "Log rotation: keep %u is out of 0...99", a_keep);

	if ( !a_limit || !s_logpathlen || (g_logoutput <= STDERR_FILENO) )
		return	STS$K_SUCCESS;

	if ( 0 > fstat(g_logoutput, &l_st) )
		return	$LOG(STS$K_ERROR, "Log rotation: fstat(%d) -> %d", g_logoutput, errno);

	if ( (size_t) l_st.st_size < a_limit )
		return	STS$K_SUCCESS;

	/* Only one thread rotates; the others find it under way and go on	*/
	if ( $LOGROT_CLAIM() )
		return	STS$K_SUCCESS;

	if ( !a_keep )
		{
		/* O_APPEND: the next record goes to the new end, offset 0	*/
		if ( 0 > ftruncate(g_logoutput, 0) )
			l_sts = $LOG(STS$K_ERROR, "Log rotation: ftruncate(%d, 0) -> %d", g_logoutput, errno);
		else	$LOG(STS$K_INFO, "Log file '%s' has been truncated at %llu octets.", s_logpath,
				(unsigned long long) l_st.st_size);

		$LOGROT_RELEASE();

		return	l_sts;
		}

	/* <file>.<keep-1> -> <file>.<keep>, ..., <file>.1 -> <file>.2; a gap
	 * in the row (fewer files than keep so far) is not an error.
	 */
	for ( unsigned i = a_keep - 1; i; i-- )
		{
		snprintf(l_old, sizeof(l_old), "%s.%u", s_logpath, i);
		snprintf(l_new, sizeof(l_new), "%s.%u", s_logpath, i + 1);
#ifdef	_WIN32
		remove(l_new);
#endif
		if ( (0 > rename(l_old, l_new)) && (ENOENT != errno) )
			$LOG(STS$K_WARN, "Log rotation: rename('%s', '%s') -> %d", l_old, l_new, errno);
		}

	snprintf(l_new, sizeof(l_new), "%s.1", s_logpath);
#ifdef	_WIN32
	remove(l_new);
#endif

	if ( 0 > rename(s_logpath, l_new) )
		{
		l_sts = $LOG(STS$K_ERROR, "Log rotation: rename('%s', '%s') -> %d", s_logpath, l_new, errno);
		$LOGROT_RELEASE();

		return	l_sts;
		}

	/* The old file is <file>.1 now and still open behind the descriptor:
	 * the records written until the dup2() below land there, in order.
	 */
	if ( 0 > (l_fd = open(s_logpath, O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC, l_mode)) )
		{
		l_sts = $LOG(STS$K_ERROR, "Log rotation: open('%s') -> %d, logging goes on into '%s'", s_logpath, errno, l_new);
		$LOGROT_RELEASE();

		return	l_sts;
		}

	if ( 0 > dup2(l_fd, g_logoutput) )
		{
		l_sts = $LOG(STS$K_ERROR, "Log rotation: dup2(%d, %d) -> %d, logging goes on into '%s'", l_fd, g_logoutput, errno, l_new);
		close (l_fd);
		$LOGROT_RELEASE();

		return	l_sts;
		}

	dup2(l_fd, STDOUT_FILENO);
	dup2(l_fd, STDERR_FILENO);
	close (l_fd);

	$LOGROT_RELEASE();

	return	$LOG(STS$K_INFO, "Log file '%s' has been rotated at %llu octets, the previous one is '%s'.", s_logpath,
			(unsigned long long) l_st.st_size, l_new);
}



/*
	http://util.deltatelecom.ru/ovms82src/debug/lis/strings.lis

				Source Listing                  19-NOV-2004 08:39:18  Compaq C V6.5-001-48BCD           Page 42
								 4-MAR-2003 15:31:10  $1$DGA1016:[DEBUG_2.SRC]STRINGS.C;1
*/

/*++***********************************************************************/
/*
 * FUNCTION : C_STR$pattern_match
 *
 * FUNCTIONALITY :
 *  This function is passed a pattern and a string to match against the
 *  pattern.  The pattern may contain wildcards (*,%), where '*' matches
 *  0 or more occurrences of anything, and '%' matches any one character.
 *  In addition, a '\' in the pattern string will translate the next
 *  character as literal.  This is a recursive routine.
 *
 * COMMUNICATION :
 *
 *  PROTOTYPE/PARAMETERS :
 *      (see below)
 *
 *  RETURN VALUE :
 *      int : success or failure
 *
 *  ERROR HANDLING :
 *
 * ALGORITHM :
 *  establish a pointer to the first character in the magic string
 *  and a pointer to the first character in the text string.
 *
 *   while there are still characters:
 *    If the current character in the magic string is not a wild card
 *    and they are different, then declare it a mismatch.  If they
 *    point to identical characters, then advance each pointer by one
 *    and go around the loop again.
 *
 *    If the magic string pointer points at a WILD_PERCENT, then advance
 *    both pointers, regardless of what's in the test string.
 *
 *    If the magic pointer points to WILD_STAR, then try matching the
 *    rest of the magic string at each character position in the text
 *    string by calling C_STR$pattern_match (i.e. this function)
 *    recursively.  If any of those succeed then declare this one a success.
 *
 * CAVEATS :
 *  (1) pattern$ is assumed to be preprocessed, i.e, * replaced by WILD_STAR,
 *     '%' replaced by WILD_PERCENT ('?' is accepted as its alias),
 *     '\<character>' by '<character>', etc.
 *
 *  (2) str$, however, is LITERAL, ie, it does not have wildcards or
 *      '\<character>', etc.  *, %, and \ are given no special treatment.
 *
 * HISTORY :
 *
 *  V0 - V7
 *
 *  12-JUL-89   A. Seigel
 *      (1) Created this function.
 *
 *  11-SEP-89   A. Seigel
 *      (1) Fixed bug with literals (\) in pattern$.
 *

		       Source Listing                  19-NOV-2004 08:39:18  Compaq C V6.5-001-48BCD           Page 43
							4-MAR-2003 15:31:10  $1$DGA1016:[DEBUG_2.SRC]STRINGS.C;1

 *  V8
 *
 *  24-MAY-1990        J. Bell
 *      (1) ANSI-ized declaration.
 *
 *
 *	27-NOV-2015	RRL	Some reformatting.
 */
/*--***********************************************************************/

#define WILD_PERCENT	'%'
#define WILD_QUESTION	'?'	/* An alias of WILD_PERCENT - one octet */
#define WILD_STAR	'*'

#ifndef	FALSE
#define	FALSE	0
#endif

#ifndef	TRUE
#define	TRUE	1
#endif


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Test whether a string matches a wildcard pattern, recursively. '*'
**	matches any run of characters and '%' matches exactly one character.
**	The comparison is case-sensitive.
**
**  FORMAL PARAMETERS:
**
**	str$		Zero-terminated subject string
**	pattern$	Zero-terminated pattern, with '*' and '%' wildcards
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	Non-zero if the string matches the pattern, zero otherwise.
**--
*/
int	__util$pattern_match	(
		char	*	str$,        /* A LITERAL string to match */
		char	*	pattern$     /* Pattern, with potential wildcards (*,%) to * match the string.*/
				)
{
const char *l_s = str$, *l_p = pattern$,
	*l_sbk = NULL, *l_pbk = NULL;	/* The single backtrack point of the last '*' seen */

	/*
	 * The iterative matcher: advance both cursors on a literal or a '%' hit;
	 * on a '*' remember the backtrack point and try the shortest run first;
	 * on a mismatch fall back to the last '*' and let it swallow one more
	 * octet. One live backtrack point suffices for the star semantics, which
	 * bounds the work by O(|str| * |pattern|) - the recursive predecessor
	 * exploded combinatorially on several stars (a*a*a*a*b against a run of
	 * 'a' took seconds); this one is linear in practice.
	 */
	while ( *l_s )
		{
		if ( (*l_p == WILD_PERCENT) || (*l_p == WILD_QUESTION) || (*l_p == *l_s) )
			{			/* '%' (or its '?' alias) takes exactly one octet; a literal must equal */
			l_p++;
			l_s++;
			}
		else if ( *l_p == WILD_STAR )
			{			/* Remember the star; first try it empty	*/
			l_pbk = l_p++;
			l_sbk = l_s;
			}
		else if ( l_pbk )
			{			/* Mismatch: the last star swallows one more octet */
			l_p = l_pbk + 1;
			l_s = ++l_sbk;
			}
		else	return	FALSE;		/* Mismatch with no star to fall back to	*/
		}

	while ( *l_p == WILD_STAR )		/* Trailing stars match the empty tail		*/
		l_p++;

	return	( *l_p == '\0' );
}
/******************* end of C_STR$pattern_match *******************/

#if	0
#ifndef	WIN32
void	__util$traceback	(void)
{
int	i, nptrs;
void *	buffer[128];
char **	strings;

	nptrs = backtrace(buffer, sizeof(buffer));

	$LOG(STS$K_INFO, " --------------------- TRACEBACK BEGIN (%d) ------", nptrs);

	if ( !(strings = backtrace_symbols(buffer, nptrs)) )
		{
		perror("backtrace_symbols");

		exit(EXIT_FAILURE);
		}

	for (i = 0; i < nptrs; i++)
		$LOG(STS$K_INFO, "%d\t%s", i, strings[i]);

	$LOG(STS$K_INFO, " --------------------- TRACEBACK END ------");

}

#endif
#endif


#ifndef	WIN32
#pragma GCC diagnostic pop
#endif





/*
 *  NOTE: despite the "crc32c" name, this routine computes the standard IEEE 802.3 / zlib
 *  CRC-32 (reflected polynomial 0xEDB88320), NOT Castagnoli CRC-32C. The check value
 *  __util$crc32c(0, "123456789", 9) == 0xCBF43926 confirms IEEE. The name is kept as-is
 *  for backward compatibility with existing callers.
 */

/*
**  CRC-32 - __util$crc32c() and the engines behind it.
**
**  NOTE: despite the "crc32c" name, the routine computes the standard IEEE 802.3 / zlib
**  CRC-32 (reflected polynomial 0xEDB88320), NOT Castagnoli CRC-32C. The check value
**  __util$crc32c(0, "123456789", 9) == 0xCBF43926 confirms IEEE. The name is kept as-is
**  for backward compatibility with existing callers.
**
**  Three engines compute the very same value; the fastest the processor has is picked on
**  the first call, at run time, so the result does not depend on the flags the library
**  was compiled with:
**
**	x86-64	PCLMULQDQ folding, after Intel's "Fast CRC Computation for Generic
**		Polynomials Using PCLMULQDQ Instruction" - the method of the Linux kernel
**		and of zlib-ng for this polynomial; four 128-bit lanes, then Barrett.
**	ARMv8	the CRC32 instructions (crc32b/crc32x - the IEEE ones, not crc32c*), asked
**		for with a target attribute and used only when AT_HWCAP says HWCAP_CRC32.
**		Before V.01-32 they were used only under __ARM_FEATURE_CRC32, which the
**		default -march of Debian and Armbian does not define.
**	any	slice-by-8 tables, endian-neutral.
**
**  STARLET_CRC32=SOFTWARE in the environment forces the tables - for a test that pins the
**  engines against each other, or to rule an engine out when looking into a problem.
*/
#include	<stdint.h>

#define	UTIL$K_CRC_SW		0			/* Slice-by-8					*/
#define	UTIL$K_CRC_CLMUL	1			/* x86-64 PCLMULQDQ				*/
#define	UTIL$K_CRC_ARM		2			/* ARMv8 CRC32 instructions			*/

/*
 *  Slice-by-8 tables. Slice 0 is the classic reflected CRC-32 (0xEDB88320) byte table;
 *  slices 1..7 are derived from it. The bytes are composed little-endian explicitly in the
 *  loop below, so the routine is endian-neutral (verified on MIPS big-endian).
 */
static const unsigned int s_crc_tab8 [8][256] = {
	{
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
	},
	{
	0x00000000, 0x191b3141, 0x32366282, 0x2b2d53c3, 0x646cc504, 0x7d77f445,
	0x565aa786, 0x4f4196c7, 0xc8d98a08, 0xd1c2bb49, 0xfaefe88a, 0xe3f4d9cb,
	0xacb54f0c, 0xb5ae7e4d, 0x9e832d8e, 0x87981ccf, 0x4ac21251, 0x53d92310,
	0x78f470d3, 0x61ef4192, 0x2eaed755, 0x37b5e614, 0x1c98b5d7, 0x05838496,
	0x821b9859, 0x9b00a918, 0xb02dfadb, 0xa936cb9a, 0xe6775d5d, 0xff6c6c1c,
	0xd4413fdf, 0xcd5a0e9e, 0x958424a2, 0x8c9f15e3, 0xa7b24620, 0xbea97761,
	0xf1e8e1a6, 0xe8f3d0e7, 0xc3de8324, 0xdac5b265, 0x5d5daeaa, 0x44469feb,
	0x6f6bcc28, 0x7670fd69, 0x39316bae, 0x202a5aef, 0x0b07092c, 0x121c386d,
	0xdf4636f3, 0xc65d07b2, 0xed705471, 0xf46b6530, 0xbb2af3f7, 0xa231c2b6,
	0x891c9175, 0x9007a034, 0x179fbcfb, 0x0e848dba, 0x25a9de79, 0x3cb2ef38,
	0x73f379ff, 0x6ae848be, 0x41c51b7d, 0x58de2a3c, 0xf0794f05, 0xe9627e44,
	0xc24f2d87, 0xdb541cc6, 0x94158a01, 0x8d0ebb40, 0xa623e883, 0xbf38d9c2,
	0x38a0c50d, 0x21bbf44c, 0x0a96a78f, 0x138d96ce, 0x5ccc0009, 0x45d73148,
	0x6efa628b, 0x77e153ca, 0xbabb5d54, 0xa3a06c15, 0x888d3fd6, 0x91960e97,
	0xded79850, 0xc7cca911, 0xece1fad2, 0xf5facb93, 0x7262d75c, 0x6b79e61d,
	0x4054b5de, 0x594f849f, 0x160e1258, 0x0f152319, 0x243870da, 0x3d23419b,
	0x65fd6ba7, 0x7ce65ae6, 0x57cb0925, 0x4ed03864, 0x0191aea3, 0x188a9fe2,
	0x33a7cc21, 0x2abcfd60, 0xad24e1af, 0xb43fd0ee, 0x9f12832d, 0x8609b26c,
	0xc94824ab, 0xd05315ea, 0xfb7e4629, 0xe2657768, 0x2f3f79f6, 0x362448b7,
	0x1d091b74, 0x04122a35, 0x4b53bcf2, 0x52488db3, 0x7965de70, 0x607eef31,
	0xe7e6f3fe, 0xfefdc2bf, 0xd5d0917c, 0xcccba03d, 0x838a36fa, 0x9a9107bb,
	0xb1bc5478, 0xa8a76539, 0x3b83984b, 0x2298a90a, 0x09b5fac9, 0x10aecb88,
	0x5fef5d4f, 0x46f46c0e, 0x6dd93fcd, 0x74c20e8c, 0xf35a1243, 0xea412302,
	0xc16c70c1, 0xd8774180, 0x9736d747, 0x8e2de606, 0xa500b5c5, 0xbc1b8484,
	0x71418a1a, 0x685abb5b, 0x4377e898, 0x5a6cd9d9, 0x152d4f1e, 0x0c367e5f,
	0x271b2d9c, 0x3e001cdd, 0xb9980012, 0xa0833153, 0x8bae6290, 0x92b553d1,
	0xddf4c516, 0xc4eff457, 0xefc2a794, 0xf6d996d5, 0xae07bce9, 0xb71c8da8,
	0x9c31de6b, 0x852aef2a, 0xca6b79ed, 0xd37048ac, 0xf85d1b6f, 0xe1462a2e,
	0x66de36e1, 0x7fc507a0, 0x54e85463, 0x4df36522, 0x02b2f3e5, 0x1ba9c2a4,
	0x30849167, 0x299fa026, 0xe4c5aeb8, 0xfdde9ff9, 0xd6f3cc3a, 0xcfe8fd7b,
	0x80a96bbc, 0x99b25afd, 0xb29f093e, 0xab84387f, 0x2c1c24b0, 0x350715f1,
	0x1e2a4632, 0x07317773, 0x4870e1b4, 0x516bd0f5, 0x7a468336, 0x635db277,
	0xcbfad74e, 0xd2e1e60f, 0xf9ccb5cc, 0xe0d7848d, 0xaf96124a, 0xb68d230b,
	0x9da070c8, 0x84bb4189, 0x03235d46, 0x1a386c07, 0x31153fc4, 0x280e0e85,
	0x674f9842, 0x7e54a903, 0x5579fac0, 0x4c62cb81, 0x8138c51f, 0x9823f45e,
	0xb30ea79d, 0xaa1596dc, 0xe554001b, 0xfc4f315a, 0xd7626299, 0xce7953d8,
	0x49e14f17, 0x50fa7e56, 0x7bd72d95, 0x62cc1cd4, 0x2d8d8a13, 0x3496bb52,
	0x1fbbe891, 0x06a0d9d0, 0x5e7ef3ec, 0x4765c2ad, 0x6c48916e, 0x7553a02f,
	0x3a1236e8, 0x230907a9, 0x0824546a, 0x113f652b, 0x96a779e4, 0x8fbc48a5,
	0xa4911b66, 0xbd8a2a27, 0xf2cbbce0, 0xebd08da1, 0xc0fdde62, 0xd9e6ef23,
	0x14bce1bd, 0x0da7d0fc, 0x268a833f, 0x3f91b27e, 0x70d024b9, 0x69cb15f8,
	0x42e6463b, 0x5bfd777a, 0xdc656bb5, 0xc57e5af4, 0xee530937, 0xf7483876,
	0xb809aeb1, 0xa1129ff0, 0x8a3fcc33, 0x9324fd72
	},
	{
	0x00000000, 0x01c26a37, 0x0384d46e, 0x0246be59, 0x0709a8dc, 0x06cbc2eb,
	0x048d7cb2, 0x054f1685, 0x0e1351b8, 0x0fd13b8f, 0x0d9785d6, 0x0c55efe1,
	0x091af964, 0x08d89353, 0x0a9e2d0a, 0x0b5c473d, 0x1c26a370, 0x1de4c947,
	0x1fa2771e, 0x1e601d29, 0x1b2f0bac, 0x1aed619b, 0x18abdfc2, 0x1969b5f5,
	0x1235f2c8, 0x13f798ff, 0x11b126a6, 0x10734c91, 0x153c5a14, 0x14fe3023,
	0x16b88e7a, 0x177ae44d, 0x384d46e0, 0x398f2cd7, 0x3bc9928e, 0x3a0bf8b9,
	0x3f44ee3c, 0x3e86840b, 0x3cc03a52, 0x3d025065, 0x365e1758, 0x379c7d6f,
	0x35dac336, 0x3418a901, 0x3157bf84, 0x3095d5b3, 0x32d36bea, 0x331101dd,
	0x246be590, 0x25a98fa7, 0x27ef31fe, 0x262d5bc9, 0x23624d4c, 0x22a0277b,
	0x20e69922, 0x2124f315, 0x2a78b428, 0x2bbade1f, 0x29fc6046, 0x283e0a71,
	0x2d711cf4, 0x2cb376c3, 0x2ef5c89a, 0x2f37a2ad, 0x709a8dc0, 0x7158e7f7,
	0x731e59ae, 0x72dc3399, 0x7793251c, 0x76514f2b, 0x7417f172, 0x75d59b45,
	0x7e89dc78, 0x7f4bb64f, 0x7d0d0816, 0x7ccf6221, 0x798074a4, 0x78421e93,
	0x7a04a0ca, 0x7bc6cafd, 0x6cbc2eb0, 0x6d7e4487, 0x6f38fade, 0x6efa90e9,
	0x6bb5866c, 0x6a77ec5b, 0x68315202, 0x69f33835, 0x62af7f08, 0x636d153f,
	0x612bab66, 0x60e9c151, 0x65a6d7d4, 0x6464bde3, 0x662203ba, 0x67e0698d,
	0x48d7cb20, 0x4915a117, 0x4b531f4e, 0x4a917579, 0x4fde63fc, 0x4e1c09cb,
	0x4c5ab792, 0x4d98dda5, 0x46c49a98, 0x4706f0af, 0x45404ef6, 0x448224c1,
	0x41cd3244, 0x400f5873, 0x4249e62a, 0x438b8c1d, 0x54f16850, 0x55330267,
	0x5775bc3e, 0x56b7d609, 0x53f8c08c, 0x523aaabb, 0x507c14e2, 0x51be7ed5,
	0x5ae239e8, 0x5b2053df, 0x5966ed86, 0x58a487b1, 0x5deb9134, 0x5c29fb03,
	0x5e6f455a, 0x5fad2f6d, 0xe1351b80, 0xe0f771b7, 0xe2b1cfee, 0xe373a5d9,
	0xe63cb35c, 0xe7fed96b, 0xe5b86732, 0xe47a0d05, 0xef264a38, 0xeee4200f,
	0xeca29e56, 0xed60f461, 0xe82fe2e4, 0xe9ed88d3, 0xebab368a, 0xea695cbd,
	0xfd13b8f0, 0xfcd1d2c7, 0xfe976c9e, 0xff5506a9, 0xfa1a102c, 0xfbd87a1b,
	0xf99ec442, 0xf85cae75, 0xf300e948, 0xf2c2837f, 0xf0843d26, 0xf1465711,
	0xf4094194, 0xf5cb2ba3, 0xf78d95fa, 0xf64fffcd, 0xd9785d60, 0xd8ba3757,
	0xdafc890e, 0xdb3ee339, 0xde71f5bc, 0xdfb39f8b, 0xddf521d2, 0xdc374be5,
	0xd76b0cd8, 0xd6a966ef, 0xd4efd8b6, 0xd52db281, 0xd062a404, 0xd1a0ce33,
	0xd3e6706a, 0xd2241a5d, 0xc55efe10, 0xc49c9427, 0xc6da2a7e, 0xc7184049,
	0xc25756cc, 0xc3953cfb, 0xc1d382a2, 0xc011e895, 0xcb4dafa8, 0xca8fc59f,
	0xc8c97bc6, 0xc90b11f1, 0xcc440774, 0xcd866d43, 0xcfc0d31a, 0xce02b92d,
	0x91af9640, 0x906dfc77, 0x922b422e, 0x93e92819, 0x96a63e9c, 0x976454ab,
	0x9522eaf2, 0x94e080c5, 0x9fbcc7f8, 0x9e7eadcf, 0x9c381396, 0x9dfa79a1,
	0x98b56f24, 0x99770513, 0x9b31bb4a, 0x9af3d17d, 0x8d893530, 0x8c4b5f07,
	0x8e0de15e, 0x8fcf8b69, 0x8a809dec, 0x8b42f7db, 0x89044982, 0x88c623b5,
	0x839a6488, 0x82580ebf, 0x801eb0e6, 0x81dcdad1, 0x8493cc54, 0x8551a663,
	0x8717183a, 0x86d5720d, 0xa9e2d0a0, 0xa820ba97, 0xaa6604ce, 0xaba46ef9,
	0xaeeb787c, 0xaf29124b, 0xad6fac12, 0xacadc625, 0xa7f18118, 0xa633eb2f,
	0xa4755576, 0xa5b73f41, 0xa0f829c4, 0xa13a43f3, 0xa37cfdaa, 0xa2be979d,
	0xb5c473d0, 0xb40619e7, 0xb640a7be, 0xb782cd89, 0xb2cddb0c, 0xb30fb13b,
	0xb1490f62, 0xb08b6555, 0xbbd72268, 0xba15485f, 0xb853f606, 0xb9919c31,
	0xbcde8ab4, 0xbd1ce083, 0xbf5a5eda, 0xbe9834ed
	},
	{
	0x00000000, 0xb8bc6765, 0xaa09c88b, 0x12b5afee, 0x8f629757, 0x37def032,
	0x256b5fdc, 0x9dd738b9, 0xc5b428ef, 0x7d084f8a, 0x6fbde064, 0xd7018701,
	0x4ad6bfb8, 0xf26ad8dd, 0xe0df7733, 0x58631056, 0x5019579f, 0xe8a530fa,
	0xfa109f14, 0x42acf871, 0xdf7bc0c8, 0x67c7a7ad, 0x75720843, 0xcdce6f26,
	0x95ad7f70, 0x2d111815, 0x3fa4b7fb, 0x8718d09e, 0x1acfe827, 0xa2738f42,
	0xb0c620ac, 0x087a47c9, 0xa032af3e, 0x188ec85b, 0x0a3b67b5, 0xb28700d0,
	0x2f503869, 0x97ec5f0c, 0x8559f0e2, 0x3de59787, 0x658687d1, 0xdd3ae0b4,
	0xcf8f4f5a, 0x7733283f, 0xeae41086, 0x525877e3, 0x40edd80d, 0xf851bf68,
	0xf02bf8a1, 0x48979fc4, 0x5a22302a, 0xe29e574f, 0x7f496ff6, 0xc7f50893,
	0xd540a77d, 0x6dfcc018, 0x359fd04e, 0x8d23b72b, 0x9f9618c5, 0x272a7fa0,
	0xbafd4719, 0x0241207c, 0x10f48f92, 0xa848e8f7, 0x9b14583d, 0x23a83f58,
	0x311d90b6, 0x89a1f7d3, 0x1476cf6a, 0xaccaa80f, 0xbe7f07e1, 0x06c36084,
	0x5ea070d2, 0xe61c17b7, 0xf4a9b859, 0x4c15df3c, 0xd1c2e785, 0x697e80e0,
	0x7bcb2f0e, 0xc377486b, 0xcb0d0fa2, 0x73b168c7, 0x6104c729, 0xd9b8a04c,
	0x446f98f5, 0xfcd3ff90, 0xee66507e, 0x56da371b, 0x0eb9274d, 0xb6054028,
	0xa4b0efc6, 0x1c0c88a3, 0x81dbb01a, 0x3967d77f, 0x2bd27891, 0x936e1ff4,
	0x3b26f703, 0x839a9066, 0x912f3f88, 0x299358ed, 0xb4446054, 0x0cf80731,
	0x1e4da8df, 0xa6f1cfba, 0xfe92dfec, 0x462eb889, 0x549b1767, 0xec277002,
	0x71f048bb, 0xc94c2fde, 0xdbf98030, 0x6345e755, 0x6b3fa09c, 0xd383c7f9,
	0xc1366817, 0x798a0f72, 0xe45d37cb, 0x5ce150ae, 0x4e54ff40, 0xf6e89825,
	0xae8b8873, 0x1637ef16, 0x048240f8, 0xbc3e279d, 0x21e91f24, 0x99557841,
	0x8be0d7af, 0x335cb0ca, 0xed59b63b, 0x55e5d15e, 0x47507eb0, 0xffec19d5,
	0x623b216c, 0xda874609, 0xc832e9e7, 0x708e8e82, 0x28ed9ed4, 0x9051f9b1,
	0x82e4565f, 0x3a58313a, 0xa78f0983, 0x1f336ee6, 0x0d86c108, 0xb53aa66d,
	0xbd40e1a4, 0x05fc86c1, 0x1749292f, 0xaff54e4a, 0x322276f3, 0x8a9e1196,
	0x982bbe78, 0x2097d91d, 0x78f4c94b, 0xc048ae2e, 0xd2fd01c0, 0x6a4166a5,
	0xf7965e1c, 0x4f2a3979, 0x5d9f9697, 0xe523f1f2, 0x4d6b1905, 0xf5d77e60,
	0xe762d18e, 0x5fdeb6eb, 0xc2098e52, 0x7ab5e937, 0x680046d9, 0xd0bc21bc,
	0x88df31ea, 0x3063568f, 0x22d6f961, 0x9a6a9e04, 0x07bda6bd, 0xbf01c1d8,
	0xadb46e36, 0x15080953, 0x1d724e9a, 0xa5ce29ff, 0xb77b8611, 0x0fc7e174,
	0x9210d9cd, 0x2aacbea8, 0x38191146, 0x80a57623, 0xd8c66675, 0x607a0110,
	0x72cfaefe, 0xca73c99b, 0x57a4f122, 0xef189647, 0xfdad39a9, 0x45115ecc,
	0x764dee06, 0xcef18963, 0xdc44268d, 0x64f841e8, 0xf92f7951, 0x41931e34,
	0x5326b1da, 0xeb9ad6bf, 0xb3f9c6e9, 0x0b45a18c, 0x19f00e62, 0xa14c6907,
	0x3c9b51be, 0x842736db, 0x96929935, 0x2e2efe50, 0x2654b999, 0x9ee8defc,
	0x8c5d7112, 0x34e11677, 0xa9362ece, 0x118a49ab, 0x033fe645, 0xbb838120,
	0xe3e09176, 0x5b5cf613, 0x49e959fd, 0xf1553e98, 0x6c820621, 0xd43e6144,
	0xc68bceaa, 0x7e37a9cf, 0xd67f4138, 0x6ec3265d, 0x7c7689b3, 0xc4caeed6,
	0x591dd66f, 0xe1a1b10a, 0xf3141ee4, 0x4ba87981, 0x13cb69d7, 0xab770eb2,
	0xb9c2a15c, 0x017ec639, 0x9ca9fe80, 0x241599e5, 0x36a0360b, 0x8e1c516e,
	0x866616a7, 0x3eda71c2, 0x2c6fde2c, 0x94d3b949, 0x090481f0, 0xb1b8e695,
	0xa30d497b, 0x1bb12e1e, 0x43d23e48, 0xfb6e592d, 0xe9dbf6c3, 0x516791a6,
	0xccb0a91f, 0x740cce7a, 0x66b96194, 0xde0506f1
	},
	{
	0x00000000, 0x3d6029b0, 0x7ac05360, 0x47a07ad0, 0xf580a6c0, 0xc8e08f70,
	0x8f40f5a0, 0xb220dc10, 0x30704bc1, 0x0d106271, 0x4ab018a1, 0x77d03111,
	0xc5f0ed01, 0xf890c4b1, 0xbf30be61, 0x825097d1, 0x60e09782, 0x5d80be32,
	0x1a20c4e2, 0x2740ed52, 0x95603142, 0xa80018f2, 0xefa06222, 0xd2c04b92,
	0x5090dc43, 0x6df0f5f3, 0x2a508f23, 0x1730a693, 0xa5107a83, 0x98705333,
	0xdfd029e3, 0xe2b00053, 0xc1c12f04, 0xfca106b4, 0xbb017c64, 0x866155d4,
	0x344189c4, 0x0921a074, 0x4e81daa4, 0x73e1f314, 0xf1b164c5, 0xccd14d75,
	0x8b7137a5, 0xb6111e15, 0x0431c205, 0x3951ebb5, 0x7ef19165, 0x4391b8d5,
	0xa121b886, 0x9c419136, 0xdbe1ebe6, 0xe681c256, 0x54a11e46, 0x69c137f6,
	0x2e614d26, 0x13016496, 0x9151f347, 0xac31daf7, 0xeb91a027, 0xd6f18997,
	0x64d15587, 0x59b17c37, 0x1e1106e7, 0x23712f57, 0x58f35849, 0x659371f9,
	0x22330b29, 0x1f532299, 0xad73fe89, 0x9013d739, 0xd7b3ade9, 0xead38459,
	0x68831388, 0x55e33a38, 0x124340e8, 0x2f236958, 0x9d03b548, 0xa0639cf8,
	0xe7c3e628, 0xdaa3cf98, 0x3813cfcb, 0x0573e67b, 0x42d39cab, 0x7fb3b51b,
	0xcd93690b, 0xf0f340bb, 0xb7533a6b, 0x8a3313db, 0x0863840a, 0x3503adba,
	0x72a3d76a, 0x4fc3feda, 0xfde322ca, 0xc0830b7a, 0x872371aa, 0xba43581a,
	0x9932774d, 0xa4525efd, 0xe3f2242d, 0xde920d9d, 0x6cb2d18d, 0x51d2f83d,
	0x167282ed, 0x2b12ab5d, 0xa9423c8c, 0x9422153c, 0xd3826fec, 0xeee2465c,
	0x5cc29a4c, 0x61a2b3fc, 0x2602c92c, 0x1b62e09c, 0xf9d2e0cf, 0xc4b2c97f,
	0x8312b3af, 0xbe729a1f, 0x0c52460f, 0x31326fbf, 0x7692156f, 0x4bf23cdf,
	0xc9a2ab0e, 0xf4c282be, 0xb362f86e, 0x8e02d1de, 0x3c220dce, 0x0142247e,
	0x46e25eae, 0x7b82771e, 0xb1e6b092, 0x8c869922, 0xcb26e3f2, 0xf646ca42,
	0x44661652, 0x79063fe2, 0x3ea64532, 0x03c66c82, 0x8196fb53, 0xbcf6d2e3,
	0xfb56a833, 0xc6368183, 0x74165d93, 0x49767423, 0x0ed60ef3, 0x33b62743,
	0xd1062710, 0xec660ea0, 0xabc67470, 0x96a65dc0, 0x248681d0, 0x19e6a860,
	0x5e46d2b0, 0x6326fb00, 0xe1766cd1, 0xdc164561, 0x9bb63fb1, 0xa6d61601,
	0x14f6ca11, 0x2996e3a1, 0x6e369971, 0x5356b0c1, 0x70279f96, 0x4d47b626,
	0x0ae7ccf6, 0x3787e546, 0x85a73956, 0xb8c710e6, 0xff676a36, 0xc2074386,
	0x4057d457, 0x7d37fde7, 0x3a978737, 0x07f7ae87, 0xb5d77297, 0x88b75b27,
	0xcf1721f7, 0xf2770847, 0x10c70814, 0x2da721a4, 0x6a075b74, 0x576772c4,
	0xe547aed4, 0xd8278764, 0x9f87fdb4, 0xa2e7d404, 0x20b743d5, 0x1dd76a65,
	0x5a7710b5, 0x67173905, 0xd537e515, 0xe857cca5, 0xaff7b675, 0x92979fc5,
	0xe915e8db, 0xd475c16b, 0x93d5bbbb, 0xaeb5920b, 0x1c954e1b, 0x21f567ab,
	0x66551d7b, 0x5b3534cb, 0xd965a31a, 0xe4058aaa, 0xa3a5f07a, 0x9ec5d9ca,
	0x2ce505da, 0x11852c6a, 0x562556ba, 0x6b457f0a, 0x89f57f59, 0xb49556e9,
	0xf3352c39, 0xce550589, 0x7c75d999, 0x4115f029, 0x06b58af9, 0x3bd5a349,
	0xb9853498, 0x84e51d28, 0xc34567f8, 0xfe254e48, 0x4c059258, 0x7165bbe8,
	0x36c5c138, 0x0ba5e888, 0x28d4c7df, 0x15b4ee6f, 0x521494bf, 0x6f74bd0f,
	0xdd54611f, 0xe03448af, 0xa794327f, 0x9af41bcf, 0x18a48c1e, 0x25c4a5ae,
	0x6264df7e, 0x5f04f6ce, 0xed242ade, 0xd044036e, 0x97e479be, 0xaa84500e,
	0x4834505d, 0x755479ed, 0x32f4033d, 0x0f942a8d, 0xbdb4f69d, 0x80d4df2d,
	0xc774a5fd, 0xfa148c4d, 0x78441b9c, 0x4524322c, 0x028448fc, 0x3fe4614c,
	0x8dc4bd5c, 0xb0a494ec, 0xf704ee3c, 0xca64c78c
	},
	{
	0x00000000, 0xcb5cd3a5, 0x4dc8a10b, 0x869472ae, 0x9b914216, 0x50cd91b3,
	0xd659e31d, 0x1d0530b8, 0xec53826d, 0x270f51c8, 0xa19b2366, 0x6ac7f0c3,
	0x77c2c07b, 0xbc9e13de, 0x3a0a6170, 0xf156b2d5, 0x03d6029b, 0xc88ad13e,
	0x4e1ea390, 0x85427035, 0x9847408d, 0x531b9328, 0xd58fe186, 0x1ed33223,
	0xef8580f6, 0x24d95353, 0xa24d21fd, 0x6911f258, 0x7414c2e0, 0xbf481145,
	0x39dc63eb, 0xf280b04e, 0x07ac0536, 0xccf0d693, 0x4a64a43d, 0x81387798,
	0x9c3d4720, 0x57619485, 0xd1f5e62b, 0x1aa9358e, 0xebff875b, 0x20a354fe,
	0xa6372650, 0x6d6bf5f5, 0x706ec54d, 0xbb3216e8, 0x3da66446, 0xf6fab7e3,
	0x047a07ad, 0xcf26d408, 0x49b2a6a6, 0x82ee7503, 0x9feb45bb, 0x54b7961e,
	0xd223e4b0, 0x197f3715, 0xe82985c0, 0x23755665, 0xa5e124cb, 0x6ebdf76e,
	0x73b8c7d6, 0xb8e41473, 0x3e7066dd, 0xf52cb578, 0x0f580a6c, 0xc404d9c9,
	0x4290ab67, 0x89cc78c2, 0x94c9487a, 0x5f959bdf, 0xd901e971, 0x125d3ad4,
	0xe30b8801, 0x28575ba4, 0xaec3290a, 0x659ffaaf, 0x789aca17, 0xb3c619b2,
	0x35526b1c, 0xfe0eb8b9, 0x0c8e08f7, 0xc7d2db52, 0x4146a9fc, 0x8a1a7a59,
	0x971f4ae1, 0x5c439944, 0xdad7ebea, 0x118b384f, 0xe0dd8a9a, 0x2b81593f,
	0xad152b91, 0x6649f834, 0x7b4cc88c, 0xb0101b29, 0x36846987, 0xfdd8ba22,
	0x08f40f5a, 0xc3a8dcff, 0x453cae51, 0x8e607df4, 0x93654d4c, 0x58399ee9,
	0xdeadec47, 0x15f13fe2, 0xe4a78d37, 0x2ffb5e92, 0xa96f2c3c, 0x6233ff99,
	0x7f36cf21, 0xb46a1c84, 0x32fe6e2a, 0xf9a2bd8f, 0x0b220dc1, 0xc07ede64,
	0x46eaacca, 0x8db67f6f, 0x90b34fd7, 0x5bef9c72, 0xdd7beedc, 0x16273d79,
	0xe7718fac, 0x2c2d5c09, 0xaab92ea7, 0x61e5fd02, 0x7ce0cdba, 0xb7bc1e1f,
	0x31286cb1, 0xfa74bf14, 0x1eb014d8, 0xd5ecc77d, 0x5378b5d3, 0x98246676,
	0x852156ce, 0x4e7d856b, 0xc8e9f7c5, 0x03b52460, 0xf2e396b5, 0x39bf4510,
	0xbf2b37be, 0x7477e41b, 0x6972d4a3, 0xa22e0706, 0x24ba75a8, 0xefe6a60d,
	0x1d661643, 0xd63ac5e6, 0x50aeb748, 0x9bf264ed, 0x86f75455, 0x4dab87f0,
	0xcb3ff55e, 0x006326fb, 0xf135942e, 0x3a69478b, 0xbcfd3525, 0x77a1e680,
	0x6aa4d638, 0xa1f8059d, 0x276c7733, 0xec30a496, 0x191c11ee, 0xd240c24b,
	0x54d4b0e5, 0x9f886340, 0x828d53f8, 0x49d1805d, 0xcf45f2f3, 0x04192156,
	0xf54f9383, 0x3e134026, 0xb8873288, 0x73dbe12d, 0x6eded195, 0xa5820230,
	0x2316709e, 0xe84aa33b, 0x1aca1375, 0xd196c0d0, 0x5702b27e, 0x9c5e61db,
	0x815b5163, 0x4a0782c6, 0xcc93f068, 0x07cf23cd, 0xf6999118, 0x3dc542bd,
	0xbb513013, 0x700de3b6, 0x6d08d30e, 0xa65400ab, 0x20c07205, 0xeb9ca1a0,
	0x11e81eb4, 0xdab4cd11, 0x5c20bfbf, 0x977c6c1a, 0x8a795ca2, 0x41258f07,
	0xc7b1fda9, 0x0ced2e0c, 0xfdbb9cd9, 0x36e74f7c, 0xb0733dd2, 0x7b2fee77,
	0x662adecf, 0xad760d6a, 0x2be27fc4, 0xe0beac61, 0x123e1c2f, 0xd962cf8a,
	0x5ff6bd24, 0x94aa6e81, 0x89af5e39, 0x42f38d9c, 0xc467ff32, 0x0f3b2c97,
	0xfe6d9e42, 0x35314de7, 0xb3a53f49, 0x78f9ecec, 0x65fcdc54, 0xaea00ff1,
	0x28347d5f, 0xe368aefa, 0x16441b82, 0xdd18c827, 0x5b8cba89, 0x90d0692c,
	0x8dd55994, 0x46898a31, 0xc01df89f, 0x0b412b3a, 0xfa1799ef, 0x314b4a4a,
	0xb7df38e4, 0x7c83eb41, 0x6186dbf9, 0xaada085c, 0x2c4e7af2, 0xe712a957,
	0x15921919, 0xdececabc, 0x585ab812, 0x93066bb7, 0x8e035b0f, 0x455f88aa,
	0xc3cbfa04, 0x089729a1, 0xf9c19b74, 0x329d48d1, 0xb4093a7f, 0x7f55e9da,
	0x6250d962, 0xa90c0ac7, 0x2f987869, 0xe4c4abcc
	},
	{
	0x00000000, 0xa6770bb4, 0x979f1129, 0x31e81a9d, 0xf44f2413, 0x52382fa7,
	0x63d0353a, 0xc5a73e8e, 0x33ef4e67, 0x959845d3, 0xa4705f4e, 0x020754fa,
	0xc7a06a74, 0x61d761c0, 0x503f7b5d, 0xf64870e9, 0x67de9cce, 0xc1a9977a,
	0xf0418de7, 0x56368653, 0x9391b8dd, 0x35e6b369, 0x040ea9f4, 0xa279a240,
	0x5431d2a9, 0xf246d91d, 0xc3aec380, 0x65d9c834, 0xa07ef6ba, 0x0609fd0e,
	0x37e1e793, 0x9196ec27, 0xcfbd399c, 0x69ca3228, 0x582228b5, 0xfe552301,
	0x3bf21d8f, 0x9d85163b, 0xac6d0ca6, 0x0a1a0712, 0xfc5277fb, 0x5a257c4f,
	0x6bcd66d2, 0xcdba6d66, 0x081d53e8, 0xae6a585c, 0x9f8242c1, 0x39f54975,
	0xa863a552, 0x0e14aee6, 0x3ffcb47b, 0x998bbfcf, 0x5c2c8141, 0xfa5b8af5,
	0xcbb39068, 0x6dc49bdc, 0x9b8ceb35, 0x3dfbe081, 0x0c13fa1c, 0xaa64f1a8,
	0x6fc3cf26, 0xc9b4c492, 0xf85cde0f, 0x5e2bd5bb, 0x440b7579, 0xe27c7ecd,
	0xd3946450, 0x75e36fe4, 0xb044516a, 0x16335ade, 0x27db4043, 0x81ac4bf7,
	0x77e43b1e, 0xd19330aa, 0xe07b2a37, 0x460c2183, 0x83ab1f0d, 0x25dc14b9,
	0x14340e24, 0xb2430590, 0x23d5e9b7, 0x85a2e203, 0xb44af89e, 0x123df32a,
	0xd79acda4, 0x71edc610, 0x4005dc8d, 0xe672d739, 0x103aa7d0, 0xb64dac64,
	0x87a5b6f9, 0x21d2bd4d, 0xe47583c3, 0x42028877, 0x73ea92ea, 0xd59d995e,
	0x8bb64ce5, 0x2dc14751, 0x1c295dcc, 0xba5e5678, 0x7ff968f6, 0xd98e6342,
	0xe86679df, 0x4e11726b, 0xb8590282, 0x1e2e0936, 0x2fc613ab, 0x89b1181f,
	0x4c162691, 0xea612d25, 0xdb8937b8, 0x7dfe3c0c, 0xec68d02b, 0x4a1fdb9f,
	0x7bf7c102, 0xdd80cab6, 0x1827f438, 0xbe50ff8c, 0x8fb8e511, 0x29cfeea5,
	0xdf879e4c, 0x79f095f8, 0x48188f65, 0xee6f84d1, 0x2bc8ba5f, 0x8dbfb1eb,
	0xbc57ab76, 0x1a20a0c2, 0x8816eaf2, 0x2e61e146, 0x1f89fbdb, 0xb9fef06f,
	0x7c59cee1, 0xda2ec555, 0xebc6dfc8, 0x4db1d47c, 0xbbf9a495, 0x1d8eaf21,
	0x2c66b5bc, 0x8a11be08, 0x4fb68086, 0xe9c18b32, 0xd82991af, 0x7e5e9a1b,
	0xefc8763c, 0x49bf7d88, 0x78576715, 0xde206ca1, 0x1b87522f, 0xbdf0599b,
	0x8c184306, 0x2a6f48b2, 0xdc27385b, 0x7a5033ef, 0x4bb82972, 0xedcf22c6,
	0x28681c48, 0x8e1f17fc, 0xbff70d61, 0x198006d5, 0x47abd36e, 0xe1dcd8da,
	0xd034c247, 0x7643c9f3, 0xb3e4f77d, 0x1593fcc9, 0x247be654, 0x820cede0,
	0x74449d09, 0xd23396bd, 0xe3db8c20, 0x45ac8794, 0x800bb91a, 0x267cb2ae,
	0x1794a833, 0xb1e3a387, 0x20754fa0, 0x86024414, 0xb7ea5e89, 0x119d553d,
	0xd43a6bb3, 0x724d6007, 0x43a57a9a, 0xe5d2712e, 0x139a01c7, 0xb5ed0a73,
	0x840510ee, 0x22721b5a, 0xe7d525d4, 0x41a22e60, 0x704a34fd, 0xd63d3f49,
	0xcc1d9f8b, 0x6a6a943f, 0x5b828ea2, 0xfdf58516, 0x3852bb98, 0x9e25b02c,
	0xafcdaab1, 0x09baa105, 0xfff2d1ec, 0x5985da58, 0x686dc0c5, 0xce1acb71,
	0x0bbdf5ff, 0xadcafe4b, 0x9c22e4d6, 0x3a55ef62, 0xabc30345, 0x0db408f1,
	0x3c5c126c, 0x9a2b19d8, 0x5f8c2756, 0xf9fb2ce2, 0xc813367f, 0x6e643dcb,
	0x982c4d22, 0x3e5b4696, 0x0fb35c0b, 0xa9c457bf, 0x6c636931, 0xca146285,
	0xfbfc7818, 0x5d8b73ac, 0x03a0a617, 0xa5d7ada3, 0x943fb73e, 0x3248bc8a,
	0xf7ef8204, 0x519889b0, 0x6070932d, 0xc6079899, 0x304fe870, 0x9638e3c4,
	0xa7d0f959, 0x01a7f2ed, 0xc400cc63, 0x6277c7d7, 0x539fdd4a, 0xf5e8d6fe,
	0x647e3ad9, 0xc209316d, 0xf3e12bf0, 0x55962044, 0x90311eca, 0x3646157e,
	0x07ae0fe3, 0xa1d90457, 0x579174be, 0xf1e67f0a, 0xc00e6597, 0x66796e23,
	0xa3de50ad, 0x05a95b19, 0x34414184, 0x92364a30
	},
	{
	0x00000000, 0xccaa009e, 0x4225077d, 0x8e8f07e3, 0x844a0efa, 0x48e00e64,
	0xc66f0987, 0x0ac50919, 0xd3e51bb5, 0x1f4f1b2b, 0x91c01cc8, 0x5d6a1c56,
	0x57af154f, 0x9b0515d1, 0x158a1232, 0xd92012ac, 0x7cbb312b, 0xb01131b5,
	0x3e9e3656, 0xf23436c8, 0xf8f13fd1, 0x345b3f4f, 0xbad438ac, 0x767e3832,
	0xaf5e2a9e, 0x63f42a00, 0xed7b2de3, 0x21d12d7d, 0x2b142464, 0xe7be24fa,
	0x69312319, 0xa59b2387, 0xf9766256, 0x35dc62c8, 0xbb53652b, 0x77f965b5,
	0x7d3c6cac, 0xb1966c32, 0x3f196bd1, 0xf3b36b4f, 0x2a9379e3, 0xe639797d,
	0x68b67e9e, 0xa41c7e00, 0xaed97719, 0x62737787, 0xecfc7064, 0x205670fa,
	0x85cd537d, 0x496753e3, 0xc7e85400, 0x0b42549e, 0x01875d87, 0xcd2d5d19,
	0x43a25afa, 0x8f085a64, 0x562848c8, 0x9a824856, 0x140d4fb5, 0xd8a74f2b,
	0xd2624632, 0x1ec846ac, 0x9047414f, 0x5ced41d1, 0x299dc2ed, 0xe537c273,
	0x6bb8c590, 0xa712c50e, 0xadd7cc17, 0x617dcc89, 0xeff2cb6a, 0x2358cbf4,
	0xfa78d958, 0x36d2d9c6, 0xb85dde25, 0x74f7debb, 0x7e32d7a2, 0xb298d73c,
	0x3c17d0df, 0xf0bdd041, 0x5526f3c6, 0x998cf358, 0x1703f4bb, 0xdba9f425,
	0xd16cfd3c, 0x1dc6fda2, 0x9349fa41, 0x5fe3fadf, 0x86c3e873, 0x4a69e8ed,
	0xc4e6ef0e, 0x084cef90, 0x0289e689, 0xce23e617, 0x40ace1f4, 0x8c06e16a,
	0xd0eba0bb, 0x1c41a025, 0x92cea7c6, 0x5e64a758, 0x54a1ae41, 0x980baedf,
	0x1684a93c, 0xda2ea9a2, 0x030ebb0e, 0xcfa4bb90, 0x412bbc73, 0x8d81bced,
	0x8744b5f4, 0x4beeb56a, 0xc561b289, 0x09cbb217, 0xac509190, 0x60fa910e,
	0xee7596ed, 0x22df9673, 0x281a9f6a, 0xe4b09ff4, 0x6a3f9817, 0xa6959889,
	0x7fb58a25, 0xb31f8abb, 0x3d908d58, 0xf13a8dc6, 0xfbff84df, 0x37558441,
	0xb9da83a2, 0x7570833c, 0x533b85da, 0x9f918544, 0x111e82a7, 0xddb48239,
	0xd7718b20, 0x1bdb8bbe, 0x95548c5d, 0x59fe8cc3, 0x80de9e6f, 0x4c749ef1,
	0xc2fb9912, 0x0e51998c, 0x04949095, 0xc83e900b, 0x46b197e8, 0x8a1b9776,
	0x2f80b4f1, 0xe32ab46f, 0x6da5b38c, 0xa10fb312, 0xabcaba0b, 0x6760ba95,
	0xe9efbd76, 0x2545bde8, 0xfc65af44, 0x30cfafda, 0xbe40a839, 0x72eaa8a7,
	0x782fa1be, 0xb485a120, 0x3a0aa6c3, 0xf6a0a65d, 0xaa4de78c, 0x66e7e712,
	0xe868e0f1, 0x24c2e06f, 0x2e07e976, 0xe2ade9e8, 0x6c22ee0b, 0xa088ee95,
	0x79a8fc39, 0xb502fca7, 0x3b8dfb44, 0xf727fbda, 0xfde2f2c3, 0x3148f25d,
	0xbfc7f5be, 0x736df520, 0xd6f6d6a7, 0x1a5cd639, 0x94d3d1da, 0x5879d144,
	0x52bcd85d, 0x9e16d8c3, 0x1099df20, 0xdc33dfbe, 0x0513cd12, 0xc9b9cd8c,
	0x4736ca6f, 0x8b9ccaf1, 0x8159c3e8, 0x4df3c376, 0xc37cc495, 0x0fd6c40b,
	0x7aa64737, 0xb60c47a9, 0x3883404a, 0xf42940d4, 0xfeec49cd, 0x32464953,
	0xbcc94eb0, 0x70634e2e, 0xa9435c82, 0x65e95c1c, 0xeb665bff, 0x27cc5b61,
	0x2d095278, 0xe1a352e6, 0x6f2c5505, 0xa386559b, 0x061d761c, 0xcab77682,
	0x44387161, 0x889271ff, 0x825778e6, 0x4efd7878, 0xc0727f9b, 0x0cd87f05,
	0xd5f86da9, 0x19526d37, 0x97dd6ad4, 0x5b776a4a, 0x51b26353, 0x9d1863cd,
	0x1397642e, 0xdf3d64b0, 0x83d02561, 0x4f7a25ff, 0xc1f5221c, 0x0d5f2282,
	0x079a2b9b, 0xcb302b05, 0x45bf2ce6, 0x89152c78, 0x50353ed4, 0x9c9f3e4a,
	0x121039a9, 0xdeba3937, 0xd47f302e, 0x18d530b0, 0x965a3753, 0x5af037cd,
	0xff6b144a, 0x33c114d4, 0xbd4e1337, 0x71e413a9, 0x7b211ab0, 0xb78b1a2e,
	0x39041dcd, 0xf5ae1d53, 0x2c8e0fff, 0xe0240f61, 0x6eab0882, 0xa201081c,
	0xa8c40105, 0x646e019b, 0xeae10678, 0x264b06e6
	}
};

/*
**  The table engine, on the inner (inverted) state
*/
static unsigned int	s_util$crc32_sw (
		unsigned int	a_crc,
	const	unsigned char *	a_p,
		size_t		a_len
			)
{
unsigned int	l_crc = a_crc;

	for ( ; a_len >= 8; a_p += 8, a_len -= 8 )
		{
		l_crc ^= (unsigned) a_p[0] | ((unsigned) a_p[1] << 8) | ((unsigned) a_p[2] << 16) | ((unsigned) a_p[3] << 24);

		l_crc = s_crc_tab8[7][ l_crc        & 0xFF]
		      ^ s_crc_tab8[6][(l_crc >>  8) & 0xFF]
		      ^ s_crc_tab8[5][(l_crc >> 16) & 0xFF]
		      ^ s_crc_tab8[4][(l_crc >> 24) & 0xFF]
		      ^ s_crc_tab8[3][ a_p[4] ]
		      ^ s_crc_tab8[2][ a_p[5] ]
		      ^ s_crc_tab8[1][ a_p[6] ]
		      ^ s_crc_tab8[0][ a_p[7] ];
		}

	while ( a_len-- )						/* Tail, classic byte-at-a-time */
		l_crc = s_crc_tab8[0][(l_crc ^ *a_p++) & 0xFF] ^ (l_crc >> 8);

	return	l_crc;
}


#if	defined(__x86_64__) && defined(__GNUC__) && !defined(_WIN32)
#include	<immintrin.h>

#define	UTIL$K_CLMUL_MIN	64			/* Shortest run the folding is worth for	*/

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The PCLMULQDQ engine: fold 64 octets at a time in four 128-bit lanes by x^(4*128+64)
**	and x^(4*128), merge the lanes into one, fold 16 octets at a time, reduce 128 -> 64
**	bits and Barrett-reduce to 32. The constants are those of the reflected polynomial
**	0xEDB88320 (k1..k5, P', mu), shifted by one as the reflected domain wants them.
**
**  FORMAL PARAMETERS:
**
**	a_crc	Inner (inverted) state
**	a_p	The data
**	a_len	Its length: at least UTIL$K_CLMUL_MIN, a multiple of 16
**
**  RETURN VALUE:
**	The inner state after the data.
**--
*/
__attribute__((target("pclmul,sse4.1")))
static unsigned int	s_util$crc32_clmul (
		unsigned int	a_crc,
	const	unsigned char *	a_p,
		size_t		a_len
			)
{
static const uint64_t	l_k1k2 [2] = { 0x0154442bd4ULL, 0x01c6e41596ULL },
			l_k3k4 [2] = { 0x01751997d0ULL, 0x00ccaa009eULL },
			l_k5k0 [2] = { 0x0163cd6124ULL, 0x0000000000ULL },
			l_poly [2] = { 0x01db710641ULL, 0x01f7011641ULL };
__m128i	x0, x1, x2, x3, x4, x5, x6, x7, x8, y5, y6, y7, y8;

	x1 = _mm_loadu_si128((const __m128i *) (a_p + 0x00));
	x2 = _mm_loadu_si128((const __m128i *) (a_p + 0x10));
	x3 = _mm_loadu_si128((const __m128i *) (a_p + 0x20));
	x4 = _mm_loadu_si128((const __m128i *) (a_p + 0x30));

	x1 = _mm_xor_si128(x1, _mm_cvtsi32_si128((int) a_crc));
	x0 = _mm_loadu_si128((const __m128i *) l_k1k2);

	a_p	+= 64;
	a_len	-= 64;

	/* Four lanes, 64 octets a step */
	while ( a_len >= 64 )
		{
		x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
		x6 = _mm_clmulepi64_si128(x2, x0, 0x00);
		x7 = _mm_clmulepi64_si128(x3, x0, 0x00);
		x8 = _mm_clmulepi64_si128(x4, x0, 0x00);

		x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
		x2 = _mm_clmulepi64_si128(x2, x0, 0x11);
		x3 = _mm_clmulepi64_si128(x3, x0, 0x11);
		x4 = _mm_clmulepi64_si128(x4, x0, 0x11);

		y5 = _mm_loadu_si128((const __m128i *) (a_p + 0x00));
		y6 = _mm_loadu_si128((const __m128i *) (a_p + 0x10));
		y7 = _mm_loadu_si128((const __m128i *) (a_p + 0x20));
		y8 = _mm_loadu_si128((const __m128i *) (a_p + 0x30));

		x1 = _mm_xor_si128(_mm_xor_si128(x1, x5), y5);
		x2 = _mm_xor_si128(_mm_xor_si128(x2, x6), y6);
		x3 = _mm_xor_si128(_mm_xor_si128(x3, x7), y7);
		x4 = _mm_xor_si128(_mm_xor_si128(x4, x8), y8);

		a_p	+= 64;
		a_len	-= 64;
		}

	/* The four lanes into one */
	x0 = _mm_loadu_si128((const __m128i *) l_k3k4);

	x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
	x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
	x1 = _mm_xor_si128(_mm_xor_si128(x1, x2), x5);

	x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
	x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
	x1 = _mm_xor_si128(_mm_xor_si128(x1, x3), x5);

	x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
	x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
	x1 = _mm_xor_si128(_mm_xor_si128(x1, x4), x5);

	/* One lane, 16 octets a step */
	while ( a_len >= 16 )
		{
		x2 = _mm_loadu_si128((const __m128i *) a_p);

		x5 = _mm_clmulepi64_si128(x1, x0, 0x00);
		x1 = _mm_clmulepi64_si128(x1, x0, 0x11);
		x1 = _mm_xor_si128(_mm_xor_si128(x1, x2), x5);

		a_p	+= 16;
		a_len	-= 16;
		}

	/* 128 -> 64 bits */
	x2 = _mm_clmulepi64_si128(x1, x0, 0x10);
	x3 = _mm_setr_epi32(~0, 0, ~0, 0);
	x1 = _mm_srli_si128(x1, 8);
	x1 = _mm_xor_si128(x1, x2);

	x0 = _mm_loadl_epi64((const __m128i *) l_k5k0);

	x2 = _mm_srli_si128(x1, 4);
	x1 = _mm_and_si128(x1, x3);
	x1 = _mm_clmulepi64_si128(x1, x0, 0x00);
	x1 = _mm_xor_si128(x1, x2);

	/* Barrett: 64 -> 32 bits */
	x0 = _mm_loadu_si128((const __m128i *) l_poly);

	x2 = _mm_and_si128(x1, x3);
	x2 = _mm_clmulepi64_si128(x2, x0, 0x10);
	x2 = _mm_and_si128(x2, x3);
	x2 = _mm_clmulepi64_si128(x2, x0, 0x00);
	x1 = _mm_xor_si128(x1, x2);

	return	(unsigned int) _mm_extract_epi32(x1, 1);
}
#endif	/* __x86_64__ */


#if	defined(__aarch64__) && defined(__GNUC__) && defined(__linux__)
#include	<arm_acle.h>
#include	<sys/auxv.h>

#ifndef	HWCAP_CRC32
#define	HWCAP_CRC32	(1 << 7)			/* <asm/hwcap.h> of arm64			*/
#endif

/*
**  The ARMv8 engine: crc32x eight octets a step, crc32b for the edges. The target
**  attribute lets the instructions be emitted whatever -march the library is built with;
**  they are executed only when AT_HWCAP says the processor has them.
*/
__attribute__((target("+crc")))
static unsigned int	s_util$crc32_arm (
		unsigned int	a_crc,
	const	unsigned char *	a_p,
		size_t		a_len
			)
{
unsigned int	l_crc = a_crc;
uint64_t	l_w;

	while ( a_len && ((uintptr_t) a_p & 7) )			/* Reach an 8-byte boundary first */
		{
		l_crc = __crc32b(l_crc, *a_p++);
		a_len--;
		}

	for ( ; a_len >= 8; a_p += 8, a_len -= 8 )
		{
		memcpy(&l_w, a_p, sizeof(l_w));
		l_crc = __crc32d(l_crc, l_w);
		}

	while ( a_len-- )
		l_crc = __crc32b(l_crc, *a_p++);

	return	l_crc;
}
#endif	/* __aarch64__ */


/*
**  Which engine this processor gets: decided on the first call. Two threads deciding at
**  once decide alike, so the race is a harmless one.
*/
static int	s_util$crc_engine = -1;

static int	s_util$crc_select (void)
{
const char *	l_env = getenv("STARLET_CRC32");
int		l_engine = UTIL$K_CRC_SW;

	if ( !l_env || (strcmp(l_env, "SOFTWARE") && strcmp(l_env, "software")) )
		{
#if	defined(__x86_64__) && defined(__GNUC__) && !defined(_WIN32)
		__builtin_cpu_init();

		if ( __builtin_cpu_supports("pclmul") && __builtin_cpu_supports("sse4.1") )
			l_engine = UTIL$K_CRC_CLMUL;
#elif	defined(__aarch64__) && defined(__GNUC__) && defined(__linux__)
		if ( getauxval(AT_HWCAP) & HWCAP_CRC32 )
			l_engine = UTIL$K_CRC_ARM;
#endif
		}

	__atomic_store_n(&s_util$crc_engine, l_engine, __ATOMIC_RELAXED);

	return	l_engine;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Compute the standard IEEE 802.3 / zlib CRC-32 (reflected polynomial 0xEDB88320) of
**	a memory region, by the fastest engine the processor has. The result is the same on
**	every platform, engine and byte order; check value for "123456789" is 0xCBF43926.
**
**  FORMAL PARAMETERS:
**
**	a_crc		Initial value: 0 to start, or a previous result to continue
**	a_buf		Address of the data
**	a_buflen	Number of octets
**
**  IMPLICIT INPUTS:
**	STARLET_CRC32 of the environment, read on the first call.
**
**  RETURN VALUE:
**	The 32-bit checksum (this routine does not use the condition-value convention).
**--
*/
unsigned int	__util$crc32c (unsigned int a_crc, const void *a_buf, size_t a_buflen)
{
const unsigned char *l_p = (const unsigned char *) a_buf;
unsigned int	l_crc = a_crc ^ ~0U;
int		l_engine = __atomic_load_n(&s_util$crc_engine, __ATOMIC_RELAXED);

	if ( l_engine < 0 )
		l_engine = s_util$crc_select();

	switch ( l_engine )
		{
#if	defined(__x86_64__) && defined(__GNUC__) && !defined(_WIN32)
		case	UTIL$K_CRC_CLMUL:
			if ( a_buflen >= UTIL$K_CLMUL_MIN )
				{
				size_t	l_n = a_buflen & ~(size_t) 15;

				l_crc	  = s_util$crc32_clmul(l_crc, l_p, l_n);
				l_p	 += l_n;
				a_buflen -= l_n;
				}
			break;
#endif

#if	defined(__aarch64__) && defined(__GNUC__) && defined(__linux__)
		case	UTIL$K_CRC_ARM:
			return	s_util$crc32_arm(l_crc, l_p, a_buflen) ^ ~0U;
#endif
		}

	return	s_util$crc32_sw(l_crc, l_p, a_buflen) ^ ~0U;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Locate the first occurrence in the counted region <s1> of the counted
**	sequence <s2>, without relying on NUL termination. Both operands may
**	contain embedded NUL octets.
**
**  FORMAL PARAMETERS:
**
**	s1	Address of the region to search
**	s1len	Length of the region
**	s2	Address of the pattern
**	s2len	Length of the pattern
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE.
**
**  RETURN VALUE:
**	Address of the first occurrence, or NULL if not found.
**--
*/
char *	__util$strstr
			(
		char	* s1,
		size_t	s1len,
		char	* s2,
		size_t	s2len
			)
{
char	*p1 = s1, *p2 = s2 + 1;
size_t	p1len = s1len, cmplen = s2len - 1;

	/*
	 * Sanity check ...
	 */
	if ( !s1 || !s2 || !s1len || !s2len )
		return	NULL;

	if ( s1len < s2len )
		return	NULL;

	/*
	 * Special check for single-byte substring
	 */
	if ( !cmplen )
		return	memchr(s1, *s2, s1len);

	/*
	**  Find the first character of s2 in the remaining string.  If it is
	**  found, compare the rest of s2.
	*/
	while ( p1len && (p1 = memchr(p1, *s2, p1len)) )
		{
		/*
		 * Is the rest of the p1 is more then length of the s2 ?
		 * No ?! Stop comparing ,  return the NULL pointer.
		 */
		if ( (s1len - (p1 - s1)) < cmplen )
			return	(NULL);

		if ( !memcmp(p1 + 1,  p2, cmplen) )
			return	(p1);

		p1++;
		p1len = s1len - (p1 - s1);
		};


	/*
	**  No match.  Return the NULL pointer.
	*/
	return((char *) NULL);
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write a printf-formatted line to the log destination WITHOUT the
**	timestamp/severity prefix - for banners, tables and similar output.
**
**  FORMAL PARAMETERS:
**
**	fmt	printf-style format string
**	...	Format arguments
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	A status code.
**--
*/
unsigned	__util$out
			(
		char *	fmt,
			...
			)

{
va_list arglist;
char	out[1024] = {0};
int	olen = 0;

	va_start (arglist, fmt);
	olen += vsnprintf(out, sizeof(out) - 1, fmt, arglist);
	va_end (arglist);

	/* vsnprintf() returns the length it WOULD have written - clamp before appending */
	olen = $MIN(olen, (int) sizeof(out) - 1);

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	s_util$logwrite (g_logoutput, out, olen);

	return	STS$K_SUCCESS;
}











/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Encode a block of octets into the base64 alphabet of RFC 4648 (A-Z, a-z,
**	0-9, '+', '/'), padded with '=' to a multiple of four characters.  The
**	output is NOT zero-terminated as part of its length; a terminating NUL
**	is appended as a courtesy when the buffer has room for one beyond the
**	encoded text, so the result can be used as an ASCIZ string as well.
**
**  FORMAL PARAMETERS:
**
**	a_src		An address of the octets to encode
**	a_srclen	A number of octets to encode
**	a_dst		An address of the output buffer
**	a_dstsz		A size of the output buffer, octets
**	a_dstlen	An address to accept the length of the encoded text - it
**			is set even when the buffer is too small, so the caller
**			learns what would have been needed
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	STS$K_SUCCESS, or STS$K_ERROR on a bad argument or a buffer too small.
**--
*/
int	__util$base64	(
		void	*	a_src,
		int		a_srclen,
		void	*	a_dst,
		int		a_dstsz,
		int	*	a_dstlen
			)
{
static const char s_b64 [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const unsigned char *l_in = (const unsigned char *) a_src;
char	*l_out = (char *) a_dst;
int	l_i, l_need, l_rest;
unsigned l_b0, l_b1, l_b2;

	if ( !a_dstlen )
		return	STS$K_ERROR;

	*a_dstlen = 0;

	if ( (a_srclen < 0) || (a_dstsz < 0) || (!a_src && a_srclen) || (!a_dst && a_dstsz) )
		return	STS$K_ERROR;

	/* Four characters for every three octets, the last group padded up. */
	l_need = 4 * ((a_srclen + 2) / 3);
	*a_dstlen = l_need;

	if ( l_need > a_dstsz )
		return	STS$K_ERROR;

	for ( l_i = 0; l_i + 2 < a_srclen; l_i += 3 )
		{
		l_b0 = l_in[l_i];
		l_b1 = l_in[l_i + 1];
		l_b2 = l_in[l_i + 2];

		*(l_out++) = s_b64[ l_b0 >> 2 ];
		*(l_out++) = s_b64[ ((l_b0 & 0x03) << 4) | (l_b1 >> 4) ];
		*(l_out++) = s_b64[ ((l_b1 & 0x0f) << 2) | (l_b2 >> 6) ];
		*(l_out++) = s_b64[ l_b2 & 0x3f ];
		}

	/* The tail: one octet gives two characters and "==", two give three and "=". */
	if ( (l_rest = a_srclen - l_i) == 1 )
		{
		l_b0 = l_in[l_i];

		*(l_out++) = s_b64[ l_b0 >> 2 ];
		*(l_out++) = s_b64[ (l_b0 & 0x03) << 4 ];
		*(l_out++) = '=';
		*(l_out++) = '=';
		}
	else if ( l_rest == 2 )
		{
		l_b0 = l_in[l_i];
		l_b1 = l_in[l_i + 1];

		*(l_out++) = s_b64[ l_b0 >> 2 ];
		*(l_out++) = s_b64[ ((l_b0 & 0x03) << 4) | (l_b1 >> 4) ];
		*(l_out++) = s_b64[ (l_b1 & 0x0f) << 2 ];
		*(l_out++) = '=';
		}

	if ( l_need < a_dstsz )				/* The courtesy NUL, when there is room */
		*l_out = '\0';

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Decode base64 text (RFC 4648) back into octets.  The '=' padding is
**	accepted and it is also allowed to be absent - a trailing group of two
**	or three characters is decoded as it stands.  Everything else is an
**	error: a character outside the alphabet (white space included), a '='
**	anywhere but at the very end, more than two of them, or a trailing group
**	of a single character, which carries no whole octet.
**
**  FORMAL PARAMETERS:
**
**	a_src		An address of the text to decode
**	a_srclen	A length of the text, characters
**	a_dst		An address of the output buffer
**	a_dstsz		A size of the output buffer, octets
**	a_dstlen	An address to accept the number of octets decoded - it is
**			set even when the buffer is too small
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	STS$K_SUCCESS, or STS$K_ERROR on a bad argument, malformed text or a
**	buffer too small.
**--
*/
int	__util$unbase64	(
		void	*	a_src,
		int		a_srclen,
		void	*	a_dst,
		int		a_dstsz,
		int	*	a_dstlen
			)
{
	/* The alphabet mapped back to sextets; 0xFF marks an octet that is not in it. */
static const unsigned char s_unb64 [256] = {
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  62,0xFF,0xFF,0xFF,  63,
	  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
	  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
	  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	};
const unsigned char *l_in = (const unsigned char *) a_src;
unsigned char *l_out = (unsigned char *) a_dst;
int	l_i, l_n, l_pad = 0, l_rest, l_need, l_bits = 0;
unsigned l_acc = 0, l_v;

	if ( !a_dstlen )
		return	STS$K_ERROR;

	*a_dstlen = 0;

	if ( (a_srclen < 0) || (a_dstsz < 0) || (!a_src && a_srclen) || (!a_dst && a_dstsz) )
		return	STS$K_ERROR;

	/* Strip up to two '=' off the end; those are the only ones allowed. */
	for ( l_n = a_srclen; l_n && (l_pad < 2) && (l_in[l_n - 1] == '='); l_n--)
		l_pad++;

	/* A trailing group of one character carries no whole octet, and the
	 * padding, when present, has to complete the last group exactly. */
	if ( ((l_rest = l_n % 4) == 1) || (l_pad && ((l_rest + l_pad) != 4)) )
		return	STS$K_ERROR;

	l_need = 3 * (l_n / 4) + (l_rest ? l_rest - 1 : 0);
	*a_dstlen = l_need;

	if ( l_need > a_dstsz )
		return	STS$K_ERROR;

	/* Six bits in, eight bits out whenever there are enough of them; the
	 * one to four bits left over at the end of a partial group are dropped. */
	for ( l_i = 0; l_i < l_n; l_i++ )
		{
		if ( (l_v = s_unb64[ l_in[l_i] ]) == 0xFF )
			return	STS$K_ERROR;

		l_acc = (l_acc << 6) | l_v;
		l_bits += 6;

		if ( l_bits >= 8 )
			{
			l_bits -= 8;
			*(l_out++) = (unsigned char) (l_acc >> l_bits);
			}
		}

	return	STS$K_SUCCESS;
}


#ifdef	__MAIN_FOR_DEBUG__

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__MAIN_FOR_DEBUG__ demo harness - not part of the library API.]
**
**	Entry point of the demo: declare an option table, parse the command
**	line with __util$getparams() and print the result with __util$showparams().
**
**  FORMAL PARAMETERS:
**
**	argc	Argument count
**	argv	Argument vector
**
**  RETURN VALUE:
**	Process exit status.
**--
*/
int	main (int argc, char *argv[])
{
ASC	l_logfspec = {0}, l_confspec = {0}, l_settings = {0};
int	l_trace, l_logsize;


const OPTS l_optstbl [] =					/* General CLI options		*/
	{
		{$ASCINI("config"),	&l_confspec, ASC$K_SZ,		OPTS$K_CONF},
		{$ASCINI("trace"),	&l_trace, 0,			OPTS$K_OPT},
		{$ASCINI("logfile"),	&l_logfspec, ASC$K_SZ,		OPTS$K_STR},
		{$ASCINI("logsize"),	&l_logsize, 0,			OPTS$K_INT},
		{$ASCINI("settings"),	&l_settings,  ASC$K_SZ,		OPTS$K_STR},

		OPTS_NULL
	};



	__util$getparams(argc, argv, l_optstbl);
	__util$showparams(l_optstbl);
}
#endif	/* __MAIN_FOR_DEBUG__ */


