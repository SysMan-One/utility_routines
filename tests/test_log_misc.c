/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  The logging odds and ends of utility_routines.c.  Pinned: the prefix millisecond
**	fraction is true milliseconds (000...999); the severity letter and the value returned by
**	the __util$log*() family, with and without STS$M_SYSLOG (an undefined severity 5..7 returns STS$K_ERROR);
**	the [FAO ...] marker of a bad control string; the syslog call for a flagged
**	record (syslog() is replaced by a recorder); the "[mod\func:line]" location; __util$faohex()
**	with a NULL length; __util$inimsg() statuses, also with many threads; __util$syslog() with
**	no host (quiet success, no socket) and with one (socket created once for any threads).
**
**  AUTHOR:  StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  31-OCT-2026
**
**  MODULE:  test_log_misc.c
**--
*/
#define _GNU_SOURCE
#define __MODULE__ "LGMT$"

#ifdef	_WIN32
#include <stdio.h>

int main (void)
{
	printf("== logging odds and ends regression: skipped, it needs POSIX (syslog, pthread, /proc) ==\n");
	return	0;
}
#else
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "test_harness.h"
#include "utility_routines.h"
#include "starlet.h"

/* ---- syslog() is the recorder's: the libc one would write to the system journal */
static int	s_sl_calls;
static int	s_sl_prio;
static char	s_sl_text [512];

static void s_sl_record (int a_prio, const char *a_fmt, va_list a_ap)
{
	s_sl_calls++;
	s_sl_prio = a_prio;
	vsnprintf (s_sl_text, sizeof(s_sl_text), a_fmt, a_ap);
}

void	syslog (int a_prio, const char *a_fmt, ...)
{
va_list	l_ap;

	va_start (l_ap, a_fmt);
	s_sl_record (a_prio, a_fmt, l_ap);
	va_end (l_ap);
}

/* A library built with -D_FORTIFY_SOURCE reaches glibc's syslog() through this entry */
void	__syslog_chk (int a_prio, int a_flag, const char *a_fmt, ...)
{
va_list	l_ap;

	(void) a_flag;
	va_start (l_ap, a_fmt);
	s_sl_record (a_prio, a_fmt, l_ap);
	va_end (l_ap);
}

extern struct sockaddr_in	slogsock;		/* the SYSLOG host of the module, see __util$deflog() */

static int	s_logfd = -1;
static char	s_logname [] = "/tmp/test_log_misc_XXXXXX";

static void s_logreset (void)
{
	if ( ftruncate(s_logfd, 0) || (0 > lseek(s_logfd, 0, SEEK_SET)) )
		printf("  (log file reset failed)\n");

	s_sl_calls = 0;
}

static const char *s_logtext (void)
{
static char	l_buf [8192];
ssize_t	l_n = pread(s_logfd, l_buf, sizeof(l_buf) - 1, 0);

	l_buf[(l_n > 0) ? l_n : 0] = '\0';
	return	l_buf;
}

static int s_nfd (void)
{
DIR	*l_d = opendir("/proc/self/fd");
int	l_n = 0;

	while ( readdir(l_d) )
		l_n++;

	closedir(l_d);
	return	l_n;
}

/* ---------------------------------------------------------------------------------------------
** The fraction of the record prefix
*/
static void s_fraction (void)
{
struct timespec	l_t0, l_t1;
const char	*l_t;
int	l_ms0, l_ms1, l_f, l_try;

	for ( l_try = 0; l_try < 3; l_try++ )
		{
		/* Wait for the 800th millisecond of a second: nsec >> 20 would give 762 there */
		do	{
			clock_gettime(CLOCK_REALTIME, &l_t0);
			usleep(2000);
			}
		while ( (l_t0.tv_nsec < 800000000L) || (l_t0.tv_nsec > 850000000L) );

		s_logreset();
		__util$log("FRC", STS$K_INFO, "fraction");
		clock_gettime(CLOCK_REALTIME, &l_t1);

		if ( (l_t1.tv_sec == l_t0.tv_sec) && (l_t1.tv_nsec < 990000000L) )
			break;						/* the same second: the two clocks can be compared	*/
		}

	l_t = s_logtext();

	CHECK( (strlen(l_t) > 32) && ('.' == l_t[19]) && (' ' == l_t[23]) && isdigit((unsigned char) l_t[20]) && isdigit((unsigned char) l_t[21]) && isdigit((unsigned char) l_t[22]),
		"the prefix has a three-digit fraction: '%.30s'", l_t);

	l_f = (l_t[20] - '0') * 100 + (l_t[21] - '0') * 10 + (l_t[22] - '0');
	l_ms0 = (int) (l_t0.tv_nsec / 1000000L);
	l_ms1 = (int) (l_t1.tv_nsec / 1000000L);

	/* The log clock may be a coarse one: up to a tick (10 ms at HZ=100) behind the moment before the call, never ahead of the
	** moment after it - and nsec >> 20 is 38 ms behind at the 800th millisecond. A delay between the two readings only widens the window.
	*/
	CHECK( (l_f >= l_ms0 - 12) && (l_f <= l_ms1),
		"the fraction %03d is the millisecond of the clock (%d...%d), not nsec >> 20", l_f, l_ms0, l_ms1);
}

/* ---------------------------------------------------------------------------------------------
** The severity: letter, return value, STS$M_SYSLOG
*/
static void s_severity (void)
{
static const struct	{ unsigned in; unsigned out; char letter; int syslog; }	l_v [] = {
		{ STS$K_WARN,					STS$K_WARN,	'W', 0 },
		{ STS$K_SUCCESS,				STS$K_SUCCESS,	'S', 0 },
		{ STS$K_ERROR,					STS$K_ERROR,	'E', 0 },
		{ STS$K_INFO,					STS$K_INFO,	'I', 0 },
		{ STS$K_FATAL,					STS$K_FATAL,	'F', 0 },
		{ STS$K_ERROR | STS$M_SYSLOG,			STS$K_ERROR,	'E', 1 },
		{ STS$K_INFO | STS$M_SYSLOG,			STS$K_INFO,	'I', 1 },
		{ STS$K_FATAL | STS$M_SYSLOG,			STS$K_FATAL,	'F', 1 },
		{ STS$K_WARN | STS$M_SYSLOG,			STS$K_WARN,	'W', 1 },
		{ STS$K_SUCCESS | STS$M_SYSLOG,			STS$K_SUCCESS,	'S', 1 },
		{ 5,						STS$K_ERROR,	'?', 0 },
		{ 6,						STS$K_ERROR,	'?', 0 },
		{ 7 | STS$M_SYSLOG,				STS$K_ERROR,	'?', 1 },
		{ $DEFFAC(7) | $DEFMSG(4) | $DEFSEV(STS$K_ERROR), STS$K_ERROR, 'E', 0 },	/* a whole condition value */
		{ $DEFFAC(7) | $DEFMSG(4) | $DEFSEV(STS$K_WARN),  STS$K_WARN,  'W', 0 },
	};
int	l_i, l_bad = 0, l_badsl = 0, l_k;
unsigned l_ret [5];
char	l_buf [256], l_pat [16];
const char	*l_t;
int	l_olen;

	for ( l_i = 0; l_i < (int) (sizeof(l_v)/sizeof(l_v[0])); l_i++ )
		{
		snprintf(l_pat, sizeof(l_pat), "%%SEV-%c:", l_v[l_i].letter);

		for ( l_k = 0; l_k < 5; l_k++ )
			{
			s_logreset();

			switch ( l_k )
				{
				case 0:	l_ret[0] = __util$log("SEV", l_v[l_i].in, "text");
					break;
				case 1:	l_ret[1] = __util$log_fao("SEV", l_v[l_i].in, "text", 0);
					break;
				case 2:	l_ret[2] = __util$logd("SEV", l_v[l_i].in, "text", "MOD", "fn", 7);
					break;
				case 3:	l_ret[3] = __util$logd_fao("SEV", l_v[l_i].in, "text", "MOD", "fn", 7, 0);
					break;
				default:
					l_ret[4] = __util$log2buf(l_buf, sizeof(l_buf), &l_olen, "SEV", l_v[l_i].in, "text");
					break;
				}

			l_t = (l_k == 4) ? l_buf : s_logtext();

			if ( l_ret[l_k] != l_v[l_i].out )
				{
				l_bad++;
				printf("  routine %d, severity 0x%x: returned %u (expected %u)\n", l_k, l_v[l_i].in, l_ret[l_k], l_v[l_i].out);
				}

			/* A low bit as the severity implies: the success severities are odd, the others even; an undefined severity is an ERROR (even) */
			if ( (1 & l_ret[l_k]) != (unsigned) ((l_v[l_i].out == STS$K_SUCCESS) || (l_v[l_i].out == STS$K_INFO)) )
				l_bad++;

			if ( !strstr(l_t, "text") || (l_v[l_i].letter != '?' ? !strstr(l_t, l_pat) : !strstr(l_t, "%SEV-?:")) )
				{
				l_bad++;
				printf("  routine %d, severity 0x%x: the letter is not %c in '%s'\n", l_k, l_v[l_i].in, l_v[l_i].letter, l_t);
				}

			if ( (l_k < 4) && ((s_sl_calls != l_v[l_i].syslog) || (l_v[l_i].syslog && !strstr(s_sl_text, "text"))) )
				{
				l_badsl++;
				printf("  routine %d, severity 0x%x: syslog() was called %d times (expected %d)\n", l_k, l_v[l_i].in, s_sl_calls, l_v[l_i].syslog);
				}
			}
		}
	CHECK( 0 == l_bad, "log/log_fao/logd/logd_fao/log2buf: the letter and the returned severity (%d wrong)", l_bad);
	CHECK( 0 == l_badsl, "...STS$M_SYSLOG sends to syslog() in every one of the four, release and debug forms alike (%d wrong)", l_badsl);

	/* The priority follows the severity: informational for the odd ones, error for the others */
	s_logreset();
	__util$logd("SEV", STS$K_INFO | STS$M_SYSLOG, "x", "M", "f", 1);
	CHECK( (1 == s_sl_calls) && ((s_sl_prio & LOG_PRIMASK) == LOG_INFO), "logd: an INFO record goes to syslog at LOG_INFO (%d calls, priority %d)", s_sl_calls, s_sl_prio);
	__util$logd("SEV", STS$K_ERROR | STS$M_SYSLOG, "x", "M", "f", 1);
	CHECK( (s_sl_prio & LOG_PRIMASK) == LOG_ERR, "logd: an ERROR record goes to syslog at LOG_ERR");
}

/* ---------------------------------------------------------------------------------------------
** A bad FAO status of a record text: a marker is appended, the record is written, the return value is the severity
*/
static void s_faomark (void)
{
static EMSG_RECORD	l_rec [1];
static EMSG_RECORD_DESC	l_dsc;
const char	*l_t;
unsigned	l_rc;
char		l_big [5000];

	s_logreset();
	l_rc = __util$log_fao("FMK", STS$K_ERROR, "bad !Q here", 0);
	l_t = s_logtext();
	CHECK( (STS$K_ERROR == l_rc) && strstr(l_t, "%FMK-E: bad") && strstr(l_t, " [FAO BADDIR]\n"), "log_fao: a bad directive - the marker [FAO BADDIR], the severity returned (%u): %s", l_rc, l_t);

	s_logreset();
	l_rc = __util$log_fao("FMK", STS$K_INFO, "need !UL and !UL", 1, (fao_prm_t) 5);
	l_t = s_logtext();
	CHECK( (STS$K_INFO == l_rc) && strstr(l_t, "need 5 and") && strstr(l_t, " [FAO NOPRM]\n"), "log_fao: too few parameters - [FAO NOPRM]: %s", l_t);

	s_logreset();
	__util$logd_fao("FMK", STS$K_WARN, "x !Z", "MODX", "funcx", 5, 0);
	l_t = s_logtext();
	CHECK( strstr(l_t, "[MODX\\funcx:5] %FMK-W:  x") && strstr(l_t, " [FAO BADDIR]\n"), "logd_fao: the marker too: %s", l_t);

	s_logreset();
	l_rc = __util$log_fao("FMK", STS$K_INFO, "fine !UL", 1, (fao_prm_t) 7);
	CHECK( strstr(s_logtext(), "fine 7\n") && !strstr(s_logtext(), "[FAO"), "log_fao: a good control string gets no marker");

	/* An output buffer too small for the text: the text is cut, the marker still fits */
	memset(l_big, 'x', sizeof(l_big) - 1);
	l_big[sizeof(l_big) - 1] = '\0';
	s_logreset();
	__util$log_fao("FMK", STS$K_INFO, "!AZ", 1, (fao_prm_t) l_big);
	l_t = s_logtext();
	CHECK( strstr(l_t, "xxx") && strstr(l_t, " [FAO BUFOVF]\n") && (strlen(l_t) < 2100), "log_fao: a text longer than the record - cut, the marker still fits (%zu)", strlen(l_t));

	memset(l_rec, 0, sizeof(l_rec));
	l_rec[0].sts = (int) ($DEFFAC(0x7C0) | $DEFMSG(1) | $DEFSEV(STS$K_INFO));
	l_rec[0].textl = 8;
	memcpy(l_rec[0].text, "oops !Q.", 9);
	l_dsc.msgnr = 1;
	l_dsc.facno = 0x7C0;
	l_dsc.msgrec = l_rec;
	l_dsc.facname = "FMKF";
	CHECK( STS$K_SUCCESS == __util$inimsg(&l_dsc), "the test facility is registered");

	s_logreset();
	l_rc = __util$putmsg_fao($DEFFAC(0x7C0) | $DEFMSG(1) | $DEFSEV(STS$K_INFO), 0);
	l_t = s_logtext();
	CHECK( (l_rc == ($DEFFAC(0x7C0) | $DEFMSG(1) | $DEFSEV(STS$K_INFO))) && strstr(l_t, "oops ") && strstr(l_t, " [FAO BADDIR]\n"), "putmsg_fao: the marker, the condition value returned: %s", l_t);

	s_logreset();
	__util$putmsgd_fao($DEFFAC(0x7C0) | $DEFMSG(1) | $DEFSEV(STS$K_INFO), "M", "f", 3, 0);
	CHECK( strstr(s_logtext(), "oops ") && strstr(s_logtext(), " [FAO BADDIR]\n"), "putmsgd_fao: the marker too: %s", s_logtext());
}

/* ---------------------------------------------------------------------------------------------
** The call-site descriptor of logd, logd_fao and trace
*/
static void s_location (void)
{
	s_logreset();
	__util$logd("LOC", STS$K_INFO, "a %d", "MODX", "funcx", 41, 5);
	CHECK( strstr(s_logtext(), "[MODX\\funcx:41] %LOC-I:  a 5"), "logd: [mod\\func:line] with ONE backslash: %s", s_logtext());

	s_logreset();
	__util$logd("LOC", STS$K_INFO, "a %d", NULL, "funcx", 41, 5);
	CHECK( strstr(s_logtext(), "[funcx:41] %LOC-I:  a 5") && !strstr(s_logtext(), "null"), "logd: a NULL module gives [func:line], no (null): %s", s_logtext());

	s_logreset();
	__util$logd_fao("LOC", STS$K_INFO, "a !UL", "MODX", "funcx", 42, 1, (fao_prm_t) 6);
	CHECK( strstr(s_logtext(), "[MODX\\funcx:42] %LOC-I:  a 6"), "logd_fao: [mod\\func:line] with one backslash: %s", s_logtext());

	s_logreset();
	__util$logd_fao("LOC", STS$K_INFO, "a !UL", NULL, "funcx", 42, 1, (fao_prm_t) 6);
	CHECK( strstr(s_logtext(), "[funcx:42] %LOC-I:  a 6") && !strstr(s_logtext(), "null"), "logd_fao: a NULL module gives [func:line]: %s", s_logtext());

	s_logreset();
	__util$trace(1, "t %d", "MODX", "funcx", 43, 9);
	CHECK( strstr(s_logtext(), "[MODX\\funcx:43] ") && !strstr(s_logtext(), "\\\\"), "trace: [mod\\func:line] with ONE backslash: %s", s_logtext());

	s_logreset();
	__util$trace(1, "t %d", NULL, "funcx", 43, 9);
	CHECK( strstr(s_logtext(), "[funcx:43] ") && strstr(s_logtext(), "t 9"), "trace: a NULL module gives [func:line]");

	s_logreset();
	__util$trace(0, "never", "MODX", "funcx", 44);
	CHECK( !*s_logtext(), "trace: a false condition writes nothing");
}

/* ---------------------------------------------------------------------------------------------
** faohex
*/
static void s_faohex (void)
{
char	l_out [100];
unsigned short	l_len = 77;
int	l_n;

	memset(l_out, 'x', sizeof(l_out));
	l_n = __util$faohex("AB\x01", 3, l_out, sizeof(l_out), NULL);
	CHECK( 66 == l_n, "faohex with a NULL <outlen> works and returns the length: %d", l_n);
	CHECK( !strncmp(l_out, "41 42 01 ", 9) && !strncmp(l_out + 50, "AB.", 3), "...and formats the line");

	l_n = __util$faohex("AB", 2, l_out, 10, NULL);
	CHECK( 0 == l_n, "faohex: a small buffer and a NULL <outlen> - 0, no crash");

	l_n = __util$faohex("AB", 2, l_out, sizeof(l_out), &l_len);
	CHECK( (66 == l_n) && (66 == l_len), "faohex: <outlen> is set when it is given");

	l_n = __util$faohex("AB", 2, l_out, 10, &l_len);
	CHECK( (0 == l_n) && (0 == l_len), "faohex: ...and zeroed when the buffer is too small");
}

/* ---------------------------------------------------------------------------------------------
** inimsg
*/
#define	LGM$K_THREADS	8

#define	LGM$K_SLOTS	(2 + 2 * LGM$K_THREADS)		/* every registered descriptor stays in the list: no slot is reused */

static EMSG_RECORD	s_rec [LGM$K_SLOTS][1];
static EMSG_RECORD_DESC	s_dsc [LGM$K_SLOTS];
static pthread_barrier_t	s_bar;
static unsigned		s_ret [LGM$K_THREADS];

static void *s_inimsg_thread (void *a_arg)
{
int	l_i = (int) (long) a_arg & 0xFF, l_slot = (int) (long) a_arg >> 8;

	pthread_barrier_wait (&s_bar);
	s_ret[l_i] = __util$inimsg (&s_dsc[l_slot]);

	return	NULL;
}

static void s_setdsc (int a_i, unsigned a_facno)
{
	memset(&s_rec[a_i], 0, sizeof(s_rec[a_i]));
	s_rec[a_i][0].sts = (int) ($DEFFAC(a_facno) | $DEFMSG(1) | $DEFSEV(STS$K_INFO));
	s_rec[a_i][0].textl = 4;
	memcpy(s_rec[a_i][0].text, "mine", 5);

	memset(&s_dsc[a_i], 0, sizeof(s_dsc[a_i]));
	s_dsc[a_i].msgnr = 1;
	s_dsc[a_i].facno = a_facno;
	s_dsc[a_i].msgrec = s_rec[a_i];
	s_dsc[a_i].facname = "LGMF";
}

static void s_inimsg (void)
{
pthread_t	l_th [LGM$K_THREADS];
EMSG_RECORD	*l_found;
unsigned	l_sts;
int	l_i, l_ok, l_dup, l_fail;

	CHECK( UTIL$S_INVARG == __util$inimsg(NULL), "inimsg(NULL): UTIL$S_INVARG");
	CHECK( !(1 & UTIL$S_INVARG) && !(1 & UTIL$S_DUPFAC) && (UTIL$S_INVARG != UTIL$S_DUPFAC), "UTIL$S_INVARG and UTIL$S_DUPFAC differ and both have the low bit clear");

	s_setdsc(0, 0x7E0);
	CHECK( STS$K_SUCCESS == __util$inimsg(&s_dsc[0]), "inimsg: a new facility is registered");
	l_sts = __util$inimsg(&s_dsc[0]);
	CHECK( UTIL$S_DUPFAC == l_sts, "inimsg: the same facility again is UTIL$S_DUPFAC (got %x)", l_sts);

	s_setdsc(1, 0x7E0);
	CHECK( UTIL$S_DUPFAC == __util$inimsg(&s_dsc[1]), "inimsg: another descriptor of the same facility number is UTIL$S_DUPFAC too");

	/* The default line for a condition nobody has registered begins with '%' */
	s_logreset();
	__util$putmsg($DEFFAC(0x7E5) | $DEFMSG(9) | $DEFSEV(STS$K_ERROR));
	CHECK( strstr(s_logtext(), " %NONAME-E-NOMSG, Message number: "), "putmsg: the default line begins with '%%': %s", s_logtext());
	s_logreset();
	__util$putmsgd($DEFFAC(0x7E5) | $DEFMSG(9) | $DEFSEV(STS$K_WARN), "M", "f", 3);
	CHECK( strstr(s_logtext(), " %NONAME-W-NOMSG, ") , "putmsgd: ...so does the default line of putmsgd");
	s_logreset();
	__util$putmsg_fao($DEFFAC(0x7E5) | $DEFMSG(9) | $DEFSEV(STS$K_INFO), 0);
	CHECK( strstr(s_logtext(), " %NONAME-I-NOMSG, "), "putmsg_fao: ...and putmsg_fao");
	s_logreset();
	__util$putmsgd_fao($DEFFAC(0x7E5) | $DEFMSG(9) | $DEFSEV(STS$K_INFO), NULL, "f", 3, 0);
	CHECK( strstr(s_logtext(), " %NONAME-I-NOMSG, "), "putmsgd_fao: ...and putmsgd_fao");

	/* Eight threads, eight different facilities: all registered, all found */
	pthread_barrier_init(&s_bar, NULL, LGM$K_THREADS);
	for ( l_i = 0; l_i < LGM$K_THREADS; l_i++ )
		{
		s_setdsc(2 + l_i, 0x7F0 + (unsigned) l_i);
		s_ret[l_i] = 0xFFFF;
		pthread_create(&l_th[l_i], NULL, s_inimsg_thread, (void *) (long) (((2 + l_i) << 8) | l_i));
		}
	for ( l_i = 0; l_i < LGM$K_THREADS; l_i++ )
		pthread_join(l_th[l_i], NULL);
	pthread_barrier_destroy(&s_bar);

	for ( l_i = 0, l_ok = 0, l_fail = 0; l_i < LGM$K_THREADS; l_i++ )
		{
		l_ok += (STS$K_SUCCESS == s_ret[l_i]);
		l_fail += !(1 & __util$getmsg((unsigned) s_rec[2 + l_i][0].sts, &l_found));
		}
	CHECK( (LGM$K_THREADS == l_ok) && (0 == l_fail), "8 threads register 8 facilities at once: %d registered, %d not found", l_ok, l_fail);

	/* Eight threads, one facility: exactly one wins */
	pthread_barrier_init(&s_bar, NULL, LGM$K_THREADS);
	for ( l_i = 0; l_i < LGM$K_THREADS; l_i++ )
		{
		s_setdsc(2 + LGM$K_THREADS + l_i, 0x7FA);
		s_ret[l_i] = 0xFFFF;
		pthread_create(&l_th[l_i], NULL, s_inimsg_thread, (void *) (long) (((2 + LGM$K_THREADS + l_i) << 8) | l_i));
		}
	for ( l_i = 0; l_i < LGM$K_THREADS; l_i++ )
		pthread_join(l_th[l_i], NULL);
	pthread_barrier_destroy(&s_bar);

	for ( l_i = 0, l_ok = 0, l_dup = 0; l_i < LGM$K_THREADS; l_i++ )
		{
		l_ok += (STS$K_SUCCESS == s_ret[l_i]);
		l_dup += (UTIL$S_DUPFAC == s_ret[l_i]);
		}
	CHECK( (1 == l_ok) && (LGM$K_THREADS - 1 == l_dup), "8 threads register one facility at once: %d registered, %d duplicates", l_ok, l_dup);
}

/* ---------------------------------------------------------------------------------------------
** syslog
*/
static pthread_barrier_t	s_bar2;
static unsigned		s_sret [LGM$K_THREADS];

static void *s_syslog_thread (void *a_arg)
{
int	l_i = (int) (long) a_arg;

	pthread_barrier_wait (&s_bar2);
	s_sret[l_i] = __util$syslog (LOG_USER, LOG_DEBUG, "tag", "message", 7);

	return	NULL;
}

static void s_syslog (void)
{
int	l_fd0, l_fd1, l_i, l_ok, l_rx;
unsigned	l_sts;
pthread_t	l_th [LGM$K_THREADS];
off_t	l_end;

	/* No host: a success, and no noise on stderr, no socket */
	l_fd0 = s_nfd();
	{
	char	l_errname [] = "/tmp/test_log_misc_err_XXXXXX";
	int	l_saved = dup(STDERR_FILENO), l_e = mkstemp(l_errname);

	dup2(l_e, STDERR_FILENO);
	l_sts = __util$syslog(LOG_USER, LOG_DEBUG, "tag", "message", 7);
	l_end = lseek(l_e, 0, SEEK_END);
	dup2(l_saved, STDERR_FILENO);
	close(l_saved);
	close(l_e);
	unlink(l_errname);
	}
	l_fd1 = s_nfd();

	CHECK( STS$K_SUCCESS == l_sts, "syslog with no host is STS$K_SUCCESS (got %u)", l_sts);
	CHECK( 0 == l_end, "...and writes nothing on stderr (%ld octets)", (long) l_end);
	CHECK( l_fd0 == l_fd1, "...and opens no socket");

	/* A host which is not an address does not make one */
	__util$deflog(NULL, "no.such.host");
	l_fd0 = s_nfd();
	l_sts = __util$syslog(LOG_USER, LOG_DEBUG, "tag", "message", 7);
	CHECK( (STS$K_SUCCESS == l_sts) && (l_fd0 == s_nfd()), "deflog(NULL, \"no.such.host\") leaves the no-host state: syslog is still a quiet success");

	/* A host: the socket appears once, whatever number of threads send at the same time.
	** The host is this machine, the port a receiver of the test's own instead of 514.
	*/
	__util$deflog(NULL, "127.0.0.1");

	{
	struct sockaddr_in	l_sin;
	socklen_t	l_sinlen = sizeof(l_sin);
	struct timeval	l_tmo = { 1, 0 };

	l_rx = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&l_sin, 0, sizeof(l_sin));
	l_sin.sin_family = AF_INET;
	l_sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	CHECK( (0 <= l_rx) && (0 == bind(l_rx, (struct sockaddr *) &l_sin, sizeof(l_sin))) && (0 == getsockname(l_rx, (struct sockaddr *) &l_sin, &l_sinlen)),
		"a UDP receiver of the test");
	setsockopt(l_rx, SOL_SOCKET, SO_RCVTIMEO, &l_tmo, sizeof(l_tmo));
	slogsock.sin_port = l_sin.sin_port;
	}

	l_fd0 = s_nfd();
	pthread_barrier_init(&s_bar2, NULL, LGM$K_THREADS);
	for ( l_i = 0; l_i < LGM$K_THREADS; l_i++ )
		{
		s_sret[l_i] = 0xFFFF;
		pthread_create(&l_th[l_i], NULL, s_syslog_thread, (void *) (long) l_i);
		}
	for ( l_i = 0, l_ok = 0; l_i < LGM$K_THREADS; l_i++ )
		{
		pthread_join(l_th[l_i], NULL);
		l_ok += (STS$K_SUCCESS == s_sret[l_i]);
		}
	pthread_barrier_destroy(&s_bar2);

	CHECK( LGM$K_THREADS == l_ok, "8 threads send the first datagrams at once: all STS$K_SUCCESS (%d)", l_ok);
	CHECK( s_nfd() == l_fd0 + 1, "...and exactly one socket was created (%d -> %d descriptors)", l_fd0, s_nfd());

	/* Eight datagrams of the form "<15>              tag: message" */
	for ( l_i = 0, l_ok = 0; l_i < LGM$K_THREADS; l_i++ )
		{
		char	l_dg [256];
		ssize_t	l_n = recv(l_rx, l_dg, sizeof(l_dg) - 1, 0);

		if ( l_n > 0 )
			{
			l_dg[l_n] = '\0';
			l_ok += (NULL != strstr(l_dg, "<15> ")) && (NULL != strstr(l_dg, "tag: message")) && (l_n == (ssize_t) strlen(l_dg));
			}
		}
	CHECK( LGM$K_THREADS == l_ok, "...and the receiver gets 8 datagrams '<15> ... tag: message' (%d good)", l_ok);

	l_sts = __util$syslog(LOG_USER, LOG_DEBUG, "a-tag-much-longer-than-the-sixteen-octets-and-then-some-more-and-more-and-more-and-more-and-more-and-more-and-more-and-more", "x", 1);
	CHECK( STS$K_SUCCESS == l_sts, "syslog with a tag longer than the header buffer is sent, clamped");
	{
	char	l_dg [512];
	ssize_t	l_n = recv(l_rx, l_dg, sizeof(l_dg), 0);

	CHECK( (l_n > 0) && (l_n <= 127 + 1), "...the datagram is the 127 octets of header at most and the message (%ld octets)", (long) l_n);
	}

	l_sts = __util$syslog(LOG_USER, LOG_DEBUG, NULL, NULL, 0);
	CHECK( STS$K_SUCCESS == l_sts, "syslog with a NULL tag and message sends an empty record");
	close(l_rx);
}

int main (void)
{
	printf("== logging odds and ends regression ==\n");

	if ( 0 > (s_logfd = mkstemp(s_logname)) )
		{
		printf("  FAIL: no temporary file\n");
		return	1;
		}
	__util$setlogfd(s_logfd);

	s_syslog();			/* first: nothing has set the SYSLOG host yet */
	s_fraction();
	s_severity();
	s_faomark();
	s_location();
	s_faohex();
	s_inimsg();

	close(s_logfd);
	unlink(s_logname);

	TEST_DONE();
}

#endif	/* _WIN32 */
