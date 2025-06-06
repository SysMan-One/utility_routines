#define	__MODULE__	"UTIL$"
#define	__IDENT__	"V.01-04ECO1"
#define	__REV__		"1.04.1"


/*
**  Abstract: General purpose and utility routines
**
**  Author: Ruslan R. Laishev
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
**	25-JUN-2019	ARL	Special changes for Adnroid;
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


void (*p_cb_log_f) (const char * buf, unsigned int olen) = NULL;	/* A reference to an exteranl routine to accept
									   a has been formated message. It's is supposed to be used
									   instead of write() to a STDOUT */

struct sockaddr_in slogsock;						/* SYSLOG Host socket					*/


static EMSG_RECORD_DESC	*emsg_record_desc_root;				/* A root to the message records descriptior		*/

/*
 *   DESCRIPTION: Message record compare routine
 *
 *   INPUT:
 *
 *   OUTPUT:
 *	NONE
 *
 *   RETURNS:
 *	a - b
 */
static int __msgcmp (const void *a, const void *b)
{
const EMSG_RECORD *m1 = a, *m2 = b;


	return	( (int)m1->sts - (int)m2->sts);
}


/*
 *   DESCRIPTION: Sorting message records according <sts> field as a key,
 *	link message records descriptor in to the global list.
 *	This routine is should be called once at task initialization time before any
 *	consecutive $GETMSG/$PUTMSG calls!
 *
 *   IMPLICITE INPUTS:
 *	emsg_record_desc_root
 *
 *   INPUTS:
 *	msgdsc:	Message Records Descriptor
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE OUTPUTS:
 *	msgdsc:
 *
 *   RETURNS:
 *	STS$K_WARN	- non-principal error
 *	condition code
 */
unsigned	__util$inimsg	(EMSG_RECORD_DESC *msgdsc)
{
EMSG_RECORD_DESC *md;

	if ( !msgdsc )
		return	STS$K_WARN;

	/* At first level we try to find the message records descriptor by using facility number */
	for  (md = emsg_record_desc_root; md; md = md->link)
		if ( md->facno == msgdsc->facno)
			return	STS$K_WARN;


	qsort(msgdsc->msgrec, msgdsc->msgnr, sizeof(EMSG_RECORD), __msgcmp);

	msgdsc->link = emsg_record_desc_root;
	emsg_record_desc_root = msgdsc;

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Retrieve message record by using message number code.
 *
 *   IMPLICITE INPUTS:
 *	emsg_record_desc_root
 *
 *   INPUTS:
 *	sts:	condition code/message number code
 *
 *   OUTPUTS:
 *	outmsg:	A found message record
 *
 *   RETURNS:
 *	STS$K_SUCCESS	- outmsg contains an address of the has been found mssage record
 *	condition code
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
	if ( !(msgrec = bsearch(&sts, msgdsc->msgrec, msgdsc->msgnr, sizeof(EMSG_RECORD), __msgcmp)) )
		return	STS$K_ERROR;		/* RNF */

	*outmsg = msgrec;

	return	STS$K_SUCCESS;
}

/*
 *   DESCRIPTION: Format a message to be output on the SYS$OUTPUT by using a format from the message record
 *
 *   IMPLICITE INPUTS:
 *	emsg_record_desc_root
 *
 *   INPUTS:
 *	sts:	condition code/message number code
 *	...:	Additional parameters, according the FAO of the message
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	<sts>
 */

const char severity[STS$K_MAX]= { 'W', 'S', 'E', 'I', 'F', '?', '?', '?'};
const char defmsgfao[] = {"NONAME-%c-NOMSG, Message number: %08X [fac: %#x/%d, sev: %#x/%d, msgno: %#x/%d]"};

unsigned	__util$putmsg
			(
		unsigned	sts,
			...
			)

{
va_list arglist;
const char lfmt [] = "%02u-%02u-%04u %02u:%02u:%02u.%03u " UTIL$T_PID_FMT;
char	out[UTIL$SZ_OUTBUF + 8];
int	olen, sev;
struct tm _tm;
struct timespec now;
EMSG_RECORD *msgrec;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec <PID/TID> " prefix
	*/
	____time(&now);

#ifdef	WIN32
	localtime_s(&_tm, (time_t *)&now);
#else
	localtime_r((time_t *)&now, &_tm);
#endif

	olen = snprintf (out, UTIL$SZ_OUTBUF, lfmt,			/* Format a prefix part of the message: time + PID ... */
		_tm.tm_mday, _tm.tm_mon + 1, 1900 + _tm.tm_year,
		_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
		(unsigned) __gettid());

	if ( 1 & __util$getmsg(sts, &msgrec) )				/* Retreive the message record */
		{
		va_start (arglist, sts);				/* Format text message */
		olen += vsnprintf(out + olen, UTIL$SZ_OUTBUF - olen, msgrec->text, arglist);
		va_end (arglist);
		}
	else	{							/* Format text message with the default FAO */
		sev = $SEV(sts);
		olen += snprintf(out + olen, UTIL$SZ_OUTBUF - olen, defmsgfao, severity[sev], sts,
			$FAC(sts), $FAC(sts), sev, sev, $MSG(sts), $MSG(sts));
		}


	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	write (g_logoutput, out, olen);

	/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, " --- ", "%.*s", olen, out);
	#endif

	return	sts;
}

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
const char	lfmt [] = {"%02u-%02u-%04u %02u:%02u:%02u.%03u " UTIL$T_PID_FMT "[%s:%u] "},
	mfmt [] = {"%02u-%02u-%04u %02u:%02u:%02u.%03u "  UTIL$T_PID_FMT "[%s\\%s:%u] "};
char	out[UTIL$SZ_OUTBUF + 8];
size_t olen, sev;
struct tm _tm;
struct timespec now;
EMSG_RECORD *msgrec;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec <PID/TID> " prefix
	*/
	____time(&now);

#ifdef	WIN32
	localtime_s(&_tm, (time_t *)&now);
#else
	localtime_r((time_t *)&now, &_tm);
#endif

	olen = __mod
		? snprintf (out, UTIL$SZ_OUTBUF, mfmt, _tm.tm_mday, _tm.tm_mon + 1, 1900 + _tm.tm_year,
			_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
			(unsigned) __gettid(), __mod, __fi, __li)
		: snprintf (out, UTIL$SZ_OUTBUF, lfmt, _tm.tm_mday, _tm.tm_mon + 1, 1900 + _tm.tm_year,
			_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
			(unsigned) __gettid(), __fi, __li);

	olen = $MIN(UTIL$SZ_OUTBUF, olen);

	if ( 1 & __util$getmsg(sts, &msgrec) )				/* Retreive the message record */
		{
		va_start (arglist, __li);				/* Fromat text message */
		olen += vsnprintf(out + olen, UTIL$SZ_OUTBUF - olen, msgrec->text, arglist);
		va_end (arglist);
		}
	else	{							/* Format text message with the default FAO */
		sev = $SEV(sts);
		olen += snprintf(out + olen, UTIL$SZ_OUTBUF - olen, defmsgfao, severity[sev], sts,
			$FAC(sts), $FAC(sts), (int) sev, (int) sev, $MSG(sts), $MSG(sts));
		}


	olen = $MIN(UTIL$SZ_OUTBUF, olen);

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	write (g_logoutput, out, olen);

	/* ARL - for android logcat */
	#ifdef ANDROID_LOGCAT
		__android_log_print(ANDROID_LOG_VERBOSE, __mod, "%.*s", olen, out);
	#endif

	return	sts;
}






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
const char	__fmt [] = {"%02u-%02u-%04u %02u:%02u:%02u.%03u "  UTIL$T_PID_FMT "[%s\\%s:%u] %%%s-%c:  "};
char	out[UTIL$SZ_OUTBUF + 8];
unsigned olen, _sev = $SEV(sev);
#ifdef	__SYSLOG__
unsigned opcom = sev & STS$M_SYSLOG;
#endif
struct tm _tm = {0};
struct timespec now = {0};


	sev &= ~STS$M_SYSLOG;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]-E-:" prefix
	*/
	____time(&now);

#ifdef	WIN32
	localtime_s(&_tm, (time_t *)&now);
#else
	localtime_r((time_t *)&now, &_tm);
#endif

	olen = snprintf (out, UTIL$SZ_OUTBUF, __fmt, _tm.tm_mday, _tm.tm_mon + 1, 1900 + _tm.tm_year,
			_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
			(unsigned) __gettid(), __mod, __func, __line, fac, severity[_sev]);

	va_start (arglist, __line);
	olen += vsnprintf(out + olen, UTIL$SZ_OUTBUF - olen, fmt, arglist);
	va_end (arglist);

	olen = $MIN(olen, UTIL$SZ_OUTBUF - 1);

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	write (g_logoutput, out, olen );

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
 *
 *  Description: a simplified analog of the C RTL syslog() routine is supposed to used to
 *	format and send a message directly to IP (has been defined by $deflog();
 *
 *  Usage:
 *	__util$deflog(NULL, "172.28.4.112");
 *	...
 *	char msg [] = "GAMEOVER, all your base are belong to us";
 *	__util$syslog(LOG_KERN, LOG_DEBUG, "NOXBIT", msg, sizeof(msg));
 *
 *  Input:
 *	fac:	SYSLOG Facility code, see LOG_* constants
 *	pri:	SYSLOG Severity code, see LOG_* constants
 *	tag:	A tag string - a part of the MSG part :-)
 *	msg:	message buffer to be send
 *	msglen:	message length
 *
 *
 *  Return:
 *	STS$K_SUCCESS
 *
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
	msg_desc.name = &slogsock;
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
 *
 *  Description: out to the current SYS$OUTPUT option's name  and values.
 *
 *  Input:
 *
 *	opts:	options' table, address of array terminated by 'NUL' entry
 *
 *
 *  Return:
 *	STS$K_SUCCESS
 *
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
 *
 *  Description: processing command line arguments to extract parameters according of options table;
 *		performs basic syntax checking, copy options values to local storage for using at run-time.
 *  Input:
 *	fconf:	A configuration file to be processed
 *	opts:	options' table, address of array terminated by 'NUL' entry
 *
 *  Output:
 *	fills the 'runparams' vector by option
 *
 *  Return:
 *	STS$K_SUCCESS
 *
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
	if ( fconf && (*fconf) )
		{
		if ( !(finp = fopen(fconf, "r")) )
			return	$LOG(STS$K_ERROR, "Error open file '%s', errno = %d", fconf, errno);
		}

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
				* ((unsigned short  *) optp->ptr) = (unsigned short) atoi(valp);
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
 *
 *  Description: processing command line arguments to extract parameters according of options table;
 *		performs basic syntax checking, copy options values to local storage for using at run-time.
 *  Input:
 *	argc:	an argument count
 *	argv:	an array of command line options
 *	opts:	options' table, address of array terminated by 'NUL' entry
 *
 *  Output:
 *	fills the 'runparams' vector by option
 *
 *  Return:
 *	STS$K_SUCCESS
 *
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
		* We allow an shortened keywords but it can be a reao of conflicts.
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
				* ((unsigned short  *) optp->ptr) = (unsigned short) atoi(valp);
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
 *++
 *  Description: dump byte array to sys$output in hex, is supposed to be called indirectly by $DUMPHEX macro.
 *
 * Input:
 *	__fi:	A file name or module name string
 *	__li:	Line number in the sourve file
 *	src:	A pointer to source buffer  do be dumped
 *	srclen:	A Length of the source buffer
 *
 * Output:
 *	NONE
 * Return:
 *	NONE
 *
 *--
 */

void	__util$dumphex	(
		const char *	__fi,
		unsigned	__li,
		const void *	src,
		unsigned short	srclen
			)
{
const char	lfmt [] = {"%02u-%02u-%04u %02u:%02u:%02u.%03u [%s:%u] Dump of %u octets follows:"};
#define		UTILS$SZ_HEXWIDTH	80
char	out[256];
unsigned char *srcp = (unsigned char *) src, low, high;
unsigned olen = 0, i, j;
struct tm _tm;
struct timespec now;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]" prefix
	*/
	____time(&now);

#ifdef	WIN32
	localtime_s(&_tm, (time_t *)&now);
#else
	localtime_r((time_t *)&now, &_tm);
#endif

	olen = snprintf (out, sizeof(out) - 1, lfmt,
		_tm.tm_mday, _tm.tm_mon + 1, 1900 + _tm.tm_year,
		_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
		__fi, __li, srclen);


	/* Add <LF> at end of record*/
	olen = $MIN(sizeof(out) - 1, olen);
	out[olen++] = '\n';

	if ( p_cb_log_f )
		p_cb_log_f(out, olen);
	else	write (g_logoutput, out, olen );

	memset(out, ' ', UTILS$SZ_HEXWIDTH);

	/*
	** Format variable part of string line
	*/
	for (i = 0; i < ((srclen / 16));  i++)
		{
		olen = snprintf(out, UTILS$SZ_HEXWIDTH, "\t+%04x:  ", i * 16);
		olen = $MIN(UTILS$SZ_HEXWIDTH - 1, olen);

		memset(out + olen, ' ', UTILS$SZ_HEXWIDTH - olen);

		for (j = 0; j < 16; j++, srcp++)
			{
			high = (*srcp) >> 4;
			low = (*srcp) & 0x0f;

			out[olen + j * 3] = high + ((high < 10) ? '0' : 'a' - 10);
			out[olen + j * 3 + 1] = low + ((low < 10) ? '0' : 'a' - 10);

			out[olen + 16*3 + 2 + j] = isprint(*srcp) ? *srcp : '.';
			}

		/* Add <LF> at end of record*/
		out[UTILS$SZ_HEXWIDTH - 1] = '\n';

		/* Write to file and flush buffer depending on severity level */
		if ( p_cb_log_f )
			p_cb_log_f(out, UTILS$SZ_HEXWIDTH);
		else	write (g_logoutput, out, UTILS$SZ_HEXWIDTH );
		}

	if ( srclen % 16 )
		{
		olen = snprintf(out, UTILS$SZ_HEXWIDTH, "\t+%04x:  ", i * 16);
		olen = $MIN(UTILS$SZ_HEXWIDTH - 1, olen);

		memset(out + olen, ' ', UTILS$SZ_HEXWIDTH - olen);

		for (j = 0; j < srclen % 16; j++, srcp++)
			{
			high = (*srcp) >> 4;
			low = (*srcp) & 0x0f;

			out[olen + j * 3] = high + ((high < 10) ? '0' : 'a' - 10);
			out[olen + j * 3 + 1] = low + ((low < 10) ? '0' : 'a' - 10);

			out[olen + 16*3 + 2 + j] = isprint(*srcp) ? *srcp : '.';
			}

		/* Add <LF> at end of record*/
		out[UTILS$SZ_HEXWIDTH - 1] = '\n';

		/* Write to file and flush buffer depending on severity level */
		if ( p_cb_log_f )
			p_cb_log_f(out, UTILS$SZ_HEXWIDTH);
		else	write (g_logoutput, out, UTILS$SZ_HEXWIDTH );
		}
}


/*
 *++
 *  Description: dump byte array to sys$output in hex, is supposed to be called indirectly by $DUMPHEX macro.
 *
 * Input:
 *	src:	A pointer to source buffer  do be dumped
 *	srclen:	A Length of the source buffer
 *	dst:
 *	dstsz:
 *	dstlen:
 *
 * Return:
 *	NONE
 *
 *--
 */
int	__util$faohex	(
		void *	src,
		unsigned short	srclen,
		char *	out,
		unsigned short	outsz
			)
{
unsigned char *srcp = (unsigned char *) src, low, high;
unsigned j;

	memset(out, ' ', outsz);
	srclen = $MIN(16, srclen);

	for (j = 0; j < 16; j++, srcp++)
		{
		high = (*srcp) >> 4;
		low = (*srcp) & 0x0f;

		out[j * 3] = high + ((high < 10) ? '0' : 'a' - 10);
		out[j * 3 + 1] = low + ((low < 10) ? '0' : 'a' - 10);

		out[16*3 + 2 + j] = isprint(*srcp) ? *srcp : '.';
		}

	out[16*3 + 2 + j] = '\0';

	return	16*3 + 2 + j;
}








/*
 *++
 *  Description: print to sys$output a programm trace information, supposed to be called from
 *		$TRACE 	macro.
 *
 * Input:
 *	fmt:	A format string
 *	__fi:	A file name or module name string
 *	__li:	Line number in the source file
 *	...	Format string arguments
 * Output:
 *	NONE
 * Return:
 *	NONE
 *
 *--
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

const char	lfmt [] = {"%02u-%02u-%04u %02u:%02u:%02u.%03u "  UTIL$T_PID_FMT "[%s:%u] "},
	mfmt [] = {"%02u-%02u-%04u %02u:%02u:%02u.%03u "  UTIL$T_PID_FMT "[%s\\%s:%u] "};
char	out[1024];
int	olen, len;
struct tm _tm;
struct timespec now;

	if ( !cond )
		return;


	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]" prefix
	*/
	____time(&now);

#ifdef	WIN32
	localtime_s(&_tm, (time_t *)&now);
#else
	localtime_r((time_t *)&now, &_tm);
#endif

	olen = __mod
		? snprintf (out, sizeof(out), mfmt, _tm.tm_mday, _tm.tm_mon + 1, 1900 + _tm.tm_year,
		_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
			(unsigned) __gettid(), __mod, __fi, __li)
		: snprintf (out, sizeof(out), lfmt, _tm.tm_mday, _tm.tm_mon + 1, 1900 + _tm.tm_year,
			_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
			(unsigned) __gettid(), __fi, __li);

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
	if ( p_cb_log_f )
		p_cb_log_f(out, olen);
	else	write (g_logoutput, out, olen );



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


unsigned	__util$log
			(
		const char *	fac,
		unsigned	sev,
		const char *	fmt,
			...
			)

{
va_list arglist;
const char lfmt [] = "%02u-%02u-%04u %02u:%02u:%02u.%03u " UTIL$T_PID_FMT "%%%s-%C: ";
char	out[UTIL$SZ_OUTBUF + 8];
unsigned olen, _sev = $SEV(sev), opcom = sev & STS$M_SYSLOG;
struct tm _tm;
struct timespec now;

	/*
	** Some sanity check
	*/
	if ( !($ISINRANGE(sev, STS$K_WARN, STS$K_ERROR)) )
		sev = STS$K_UNDEF;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]<FAC>-E:" prefix
	*/
	____time(&now);

#ifdef	WIN32
	localtime_s(&_tm, (time_t *)&now);
#else
	localtime_r((time_t *)&now, &_tm);
#endif

	olen = snprintf (out, UTIL$SZ_OUTBUF, lfmt,
		_tm.tm_mday, _tm.tm_mon + 1, 1900 + _tm.tm_year,
		_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
		(unsigned) __gettid(), fac, severity[_sev]);

	va_start (arglist, fmt);
	olen += vsnprintf(out + olen, UTIL$SZ_OUTBUF - olen, fmt, arglist);
	va_end (arglist);

	olen = $MIN(olen, sizeof(out) - 1);

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	write (g_logoutput, out, olen );

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

/**
 * @brief __util$log2buf - format a message into the given output buffer
 * @param out
 * @param outsz
 * @param outlen
 * @param fac
 * @param sev
 * @param fmt
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
const char lfmt [] = "%02u-%02u-%04u %02u:%02u:%02u.%03u " UTIL$T_PID_FMT "%%%s-%C:";
char	*__outbuf = (char *) out;
unsigned	_sev = sev;
struct tm _tm;
struct timespec now;

	/*
	** Some sanity check
	*/
	if ( !($ISINRANGE(sev, STS$K_WARN, STS$K_ERROR)) )
		sev = STS$K_UNDEF;

	/*
	** Out to buffer "DD-MM-YYYY HH:MM:SS.msec [<function>\<line>]<FAC>-E:" prefix
	*/
	____time(&now);

#ifdef	WIN32
	localtime_s(&_tm, (time_t *)&now);
#else
	localtime_r((time_t *)&now, &_tm);
#endif

	*outlen = snprintf (__outbuf, outsz, lfmt,
		_tm.tm_mday, _tm.tm_mon + 1, 1900+_tm.tm_year,
		_tm.tm_hour, _tm.tm_min, _tm.tm_sec, (unsigned) now.tv_nsec/TIMSPECDEVIDER,
		(unsigned) __gettid(), fac, severity[sev]);

	va_start (arglist, fmt);
	*outlen += vsnprintf(__outbuf + *outlen, outsz - *outlen, fmt, arglist);
	va_end (arglist);

	*outlen = $MIN(*outlen, outsz);

	/* Add NIL at end of record*/
	__outbuf[*outlen] = '\0';

	return	_sev;
}

/*
 * Description: Open a file to be used as default output file for $TRACE/$LOG/$DUMPHEX routines.
 *		File will be open in shared for read and append mode!
 *
 *  Input:
 *	logfile:	a log file name string, ASCIZ
 *	sloghost:	a SYSLOG Host IP address, ASCIZ
 *
 *  Implicit output:	global logoutput variable
 *
 *  Return:
 *	condition status, see STS$ constants
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

	if ( 0 > (fd = open(logfile, O_RDWR | O_CREAT | O_APPEND, mode)) )
		return	$LOG(STS$K_ERROR, "Error opening log file '%s', errno = %d.", logfile, errno);

	lseek(fd, 0L, SEEK_END);

	g_logoutput = fd;

	dup2(fd, STDERR_FILENO);
	dup2(fd, STDOUT_FILENO);


	return $LOG(STS$K_SUCCESS, "Log file '%s' has been opened.", logfile);
}


/*
 * Description: compare a current file size with the 'limit' and rewind a current file pointer at begining of file.
 *
 *  Return:
 *	condition status, see STS$ constants
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
 *    and go aroung the loop again.
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
 *     '%' replaced by WILD_PERCENT, '\<character>' by '<character>', etc.
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
 *	27-NOV-2015	RRL	Some reformating.
 */
/*--***********************************************************************/

#define WILD_PERCENT	'%'
#define WILD_STAR	'*'

#ifndef	FALSE
#define	FALSE	0
#endif

#ifndef	TRUE
#define	TRUE	1
#endif


int	__util$pattern_match	(
		char	*	str$,        /* A LITERAL string to match */
		char	*	pattern$     /* Pattern, with potential wildcards (*,%) to * match the string.*/
				)
{
int done = FALSE, matched_to_eos = FALSE;

	while( (*pattern$ != '\0') && (!done) && ( ((*str$=='\0') && ( (*pattern$==WILD_STAR)) ) || (*str$!='\0')) )
		{
		switch (*pattern$)
			{
			case WILD_STAR:
				pattern$++;
				matched_to_eos = FALSE;
				while ( (*str$ != '\0') && (! ( matched_to_eos = __util$pattern_match( str$++, pattern$))));

				if ( matched_to_eos )
					{
					while (*str$ != '\0') str$++;
					while (*pattern$ != '\0') pattern$++;
					}
				break;

			case WILD_PERCENT:
				pattern$++;
				str$++;
				break;

			/* case '\\':*/
			/* pattern$++;*/

			default:
				if (*str$ == *pattern$)
					{
					str$++;
					pattern$++;
					}
				else done = TRUE;
			}
		}

	while (*pattern$ == WILD_STAR) pattern$++;

	return ( (*str$ == '\0') && (*pattern$ == '\0') );


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





#ifdef	__CRC32_TAB__

extern unsigned int crc32c_tab[];


#else
#define	__CRC32_TAB__	1

/*	CRC32-C	*/
static const  unsigned int __crc32c_tab__[] = {
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
};
#endif	/* __CRC32_TAB__ */

unsigned int	__util$crc32c (unsigned int crc, const void *buf, size_t buflen)
{
const unsigned char  *p = (unsigned char *) buf;

	crc = crc ^ ~0U;

	while (buflen--)
		crc = __crc32c_tab__[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return crc ^ ~0U;
}


/*
**  STRSTR/_STRSTR64
**
**  The 'strstr' function locates the first occurrence in the string pointed
**  to by 's1' of the sequence of characters in the string pointed to by 's2'.
**
**  Return values:
**
**	non-NULL    the address of the located string
**	NULL	    indicates that the string was not found
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

	/* Add <LF> at end of record*/
	out[olen++] = '\n';

	/* Write to file and flush buffer depending on severity level */
	if( p_cb_log_f )
		p_cb_log_f(out, olen);
	else	write (g_logoutput, out, olen);

	return	STS$K_SUCCESS;
}










/*
 * stc_msg_vformat
 *
 * The core formatting routine.
 * Do NOT use sprintf-style formatting codes in
 * message strings! Use the formatting directives
 * below:
 * !A{D, Z, C}
 * !U{Q, L, W, B} = unsigned quad\int64, int32, short, byte
 * !S{Q, L, W, B} = signed quad\int64, int32, short, byte
 * !X{Q, L, W, B} = hexadecimal quad\int64, int32, short, byte
 * !P = pointer
 * !T = time (DD-MMM-YYYY HH:MM:SS)
 * !D = date (DD-MMM-YYYY)
 * !! = exclamation point
 */
#ifdef	WIN32
#define	snprintf(buf, len, format,...)	_snprintf_s(buf, len, len, format, __VA_ARGS__)
#endif

static int	__util$vfao (char *a_fmt, char *a_buf, size_t a_bufsz, va_list a_ap)
{
int	l_len;
char	l_tmpbuf[256], *outp = a_buf;

	for ( ; (a_bufsz && *a_fmt); )
		{
		if (*a_fmt != '!')
			{
			*outp++ = *a_fmt++;
			a_bufsz--;

			continue;
			}

		if ( !(*(++a_fmt)) )
			break;

		/* !!
		*/
		if (*a_fmt == '!')
			{
			*outp++ = *a_fmt++;
			a_bufsz--;

			continue;
			}

		/* !X{L, W, B}, U{L, W, B}, S{L, W, B}, Q{L, W, B}
		*/
		if (*a_fmt == 'U' || *a_fmt == 'X' || *a_fmt == 'S' )
			{
			uint64_t l_val = 0;

			if (*(a_fmt+1) == 'B')
				{
				l_val = va_arg(a_ap, int);
				l_len = snprintf(outp, a_bufsz,  ((*a_fmt == 'X') ?  "0x%02X" : (*a_fmt == 'U') ? "%3u" : "%3d"),
					(unsigned char) l_val);
				}
			else if (*(a_fmt + 1) == 'W')
				{
				l_val = va_arg(a_ap, int);
				l_len = snprintf(outp, a_bufsz,  ((*a_fmt == 'X') ?  "0x%04X" : ( *a_fmt == 'U') ? "%5u" : "%6d"),
					(uint16_t) l_val);
				}
			else if (*(a_fmt + 1) == 'L')
				{
				l_val = va_arg(a_ap, uint32_t);
				l_len = snprintf(outp, a_bufsz,  ((*a_fmt == 'X') ?  "0x%08X": ( *a_fmt == 'U') ? "%u" : "%d"),
					( *a_fmt == 'U') ? (uint32_t) l_val : (int32_t) l_val);

				}
			else if (*(a_fmt + 1) == 'Q')
				{
				l_val = va_arg(a_ap, uint64_t);
				l_len = snprintf(outp, a_bufsz,  ((*a_fmt == 'X') ?  "0x%016llx": ( *a_fmt == 'U') ? "%llu" : "%lld"),
					( *a_fmt == 'U') ? (unsigned long long) l_val : (int64_t) l_val);

				}
			else	continue;

			if ( l_len < 0 )
				break;

			if (l_len >= a_bufsz)
				l_len = a_bufsz - 1;

			outp	+= l_len;
			a_bufsz	-= l_len;
			a_fmt	+= 2;

			continue;
			}




		/* !P
		*/
		if (*a_fmt == 'P')
			{
			if ( 0 > (l_len = snprintf(outp, a_bufsz, "@%p", va_arg(a_ap, void *))) )
				break;

			if (l_len >= a_bufsz)
				l_len = a_bufsz - 1;

			outp	+= l_len;
			a_bufsz	-= l_len;
			a_fmt	+= 1;

			continue;
			}

		/* !AD, !AC, !AZ ...	*/
		if (*a_fmt == 'A')
			{
			unsigned int l_slen = 0;
			char *l_ptr;

			if (*(a_fmt+1) == 'D')
				{
				l_slen	= va_arg(a_ap, unsigned int);
				l_ptr	= va_arg(a_ap, char *);

				}
			else if (*(a_fmt+1) == 'Z')
				{
				l_ptr	= va_arg(a_ap, char *);
				l_slen	= (unsigned int) strnlen(l_ptr, a_bufsz);
				}
			else if (*(a_fmt+1) == 'C')
				{
				l_ptr	= va_arg(a_ap, char *);
				l_slen	= *(l_ptr++);
				}
			else	continue;

			if (l_slen >= (unsigned int) a_bufsz)
				l_slen = (unsigned int)a_bufsz - 1;

			memcpy(outp, l_ptr, l_slen);

			outp	+= l_slen;
			a_bufsz	-= l_slen;
			a_fmt	+= 2;

			continue;
			}

		// Unrecognized directive here, just loop back up to the top
		} /* while bufsz > 0 && *fmt != null char */

	*outp = '\0';

	return (outp - a_buf);
}



int	__util$fao (void *a_fmt, void *a_buf, size_t a_bufsz, ...)
{
va_list l_ap;
int l_result;

	va_start(l_ap, a_bufsz);
	l_result = __util$vfao((char *) a_fmt, (char*) a_buf, a_bufsz, l_ap);
	va_end(l_ap);

	return l_result;
}







/*
 * Emulate VMS system services SYS$FAOL and SYS$NUMTIM (as SYS_FAOL and
 * SYS_NUMTIM)
 *
 * The following FAO directives are supported:
 *     !Sx !Ux !Zx !Ox !%S !%D !%T !AS !AZ !AC !AD !AF !n*c
 *
 * Times are represented as pointers to time_t values rather than
 * pointers to VMS time structures.
 *
 * Revised:	1-NOV-1995		Fix bug in !XL processing.
 * Revised:	3-JAN-1995		Fix bug in NUMTIM emulation (month #).
 * Revised:	15-FEB-2024	RRL	Reorganizing code to be more readable, removed SYS_NUMTIM.
 */



#ifndef VMS
typedef struct tm tm_t;
#endif


#define SS$_BUFFEROVF 1537
#define STS$K_SUCCESS 1
#define SS$_BADPARAM 20

#define MIN_INT 0x80000000
#define MIN_INT_STR "-2147483648"
#define MIN_INT_STR_L 11

#define LONG unsigned long
#define APPEND_C(c) if (j<ctx->outsize) ctx->out[j++]=c; else \
	{ ctx->outused=j; return SS$_BUFFEROVF; }

typedef struct { unsigned short l; char *a; } decsriptor_s;
typedef struct ctl_buf_t {
    char *out;				/* Output buffer */
    LONG *prmlst;			/* Parameter list */
    int ctloffset;
    int outused;			/* Output position */
    int outsize;			/* Size of output buffer */
    int width_type;
    int width;
    int value;				/* last value formatted for !%s */
} CTL$_BUF;

static int s_output_directive ( int d1, int d2, CTL$_BUF *ctx );
static int s_fmt_signed ( int value, CTL$_BUF *ctx );
static int s_fmt_unsigned ( LONG value, int base, char fill, CTL$_BUF *ctx );
static int s_fmt_pad ( char fill, int cur, CTL$_BUF *ctx );

/************************************************************************/
static int s_fmt_time (
			int	date_flag,
		struct timespec	*a_now,
		CTL$_BUF	*ctx
			)
{
int	i, j;
char	*out, buf[32];
tm_t	*local_time;
struct tm l_tm;
struct timespec l_now;

	if ( !a_now )
		____time(&l_now);
	else	l_now = *a_now;

#ifdef	WIN32
	localtime_s(&_tm, (time_t *)&now);
#else
	localtime_r((time_t *)&l_now, &l_tm);
#endif





	return	ctx->outused = j, STS$K_SUCCESS;
}




int	__util$faol (
	const decsriptor_s	*a_ctlstr,
			int	*a_outlen,
		decsriptor_s	*a_outbuf,
			LONG	*a_prmlst
		)
{
char	*l_ctl, *l_out;
CTL$_BUF l_ctx;
int	i, j, k, l_len, l_outsize, l_repcnt, l_rc;
char	l_d1, l_d2;

	/*
	* Setup for scanning string.
	*/
	l_len = a_ctlstr->l;
	l_ctl = a_ctlstr->a;
	l_ctx.outsize = l_outsize = a_outbuf->l;
	l_ctx.out = l_out = a_outbuf->a;
	l_ctx.prmlst = a_prmlst;
	l_ctx.value = 0;

	/*
	* Main loop, i is position in control string, j = output point.
	*/
	for (j = i = 0;  i < l_len; )
		{
		/*
		 * Copy characters until a directive encountered.
		 */
		while ( l_ctl[i] != '!' )
			{
			if ( j < l_outsize )
				{
				l_out[j++] = l_ctl[i++];

				if ( i >= l_len )
					return *a_outlen = j, STS$K_SUCCESS;		/* All done */
				}
			else	return	*a_outlen = l_outsize, SS$_BUFFEROVF;		/* Output buffer full */
			}


		/*
		* Parse directive, first parse field length/repeat count
		*/
		l_ctx.width = l_ctx.width_type = 0;
		l_repcnt = 0;

		for ( i++; i < l_len; i++ )
			{
			if ( l_ctl[i] == '#' )
				{
				l_ctx.width_type = 2;
				break;
				}
			else if ( (l_ctl[i] >= '0') && (l_ctl[i] <= '9') )
				{
				l_ctx.width_type = 1;
				l_ctx.width = (l_ctx.width * 10) + (int) l_ctl[i] - (int) '0';

				if ( l_ctx.width > 0x0ffff )
					return SS$_BADPARAM + 2;
				}
			else if ( l_ctl[i] == '(' )
				{
				/* Width field is really repeat count */
				l_repcnt = l_ctx.width;
				l_ctx.width = 0;

				for ( i++; i < l_len; i++ )
					{
					if ( l_ctl[i] == ')' )
						break;

					if ( l_ctl[i] == '#' )
						{
						l_ctx.width_type = 2;
						break;
						}
					else if ( (l_ctl[i] >= '0') && (l_ctl[i] <= '9') )
						{
						l_ctx.width_type = 1;
						l_ctx.width = (l_ctx.width * 10) + (int) l_ctl[i] - (int) '0';

						if ( l_ctx.width > 0x0ffff )
							return SS$_BADPARAM+4;
						}
					}
				}
			else	break;	/* end of numerics */
			}

		if ( i >= l_len )
			return SS$_BADPARAM + 6;

		if ( l_repcnt == 0 )
			l_repcnt = 1;
		else if (l_repcnt < 0)
			l_repcnt = *l_ctx.prmlst++;

		if ( l_ctx.width_type == 2 )
			l_ctx.width = *l_ctx.prmlst++;

		/*
		* Interpret directive.
		*/
		l_d1 = l_ctl[i++];
		l_d2 =  ( i < l_len ) ? l_ctl[i] : '\0';

		l_ctx.outused = j;

		for ( k = 0; k < l_repcnt; k++ )
			{
			if ( 1 != (l_rc = s_output_directive ( l_d1, l_d2, &l_ctx )) )
				return	*a_outlen = l_ctx.outused, l_rc;
			}

		j = l_ctx.outused;
		i += l_ctx.ctloffset;
		}

	return	*a_outlen = j, STS$K_SUCCESS;
}
/************************************************************************/
/* Process single directive.
 */
static int s_output_directive ( int d1, int d2, CTL$_BUF *ctx )
{
int	j, value, l_rc;
LONG	uvalue;
char	*out;

	j = ctx->outused;
	ctx->ctloffset = 1;

	/* printf("output directive d1: %d(%c), d2: %d(%c)\n", d1, d1, d2, d2 ); */
	switch ( d1 )
		{
		case 'A':
			{	/* Ascii output */
			int	srclen;
			char	*src;

			if ( d2 == 'Z' )
				{
				for ( src = (char *) *ctx->prmlst++; *src; src++ )
					{APPEND_C ( *src );}
				break;

				}
			else if ( d2 == 'D' )
				{
				srclen = *ctx->prmlst++;
				src = (char *) *ctx->prmlst++;
				}
			else if ( d2 == 'C' )
				{
				src = (char *) *ctx->prmlst++;
				srclen = (unsigned char) *src++;
				}
			else if ( d2 == 'S' )
				{
				decsriptor_s *desc = (decsriptor_s *) *ctx->prmlst++;
				srclen = desc->l; src = desc->a;
				}
			else if ( d2 == 'F' )
				{
				srclen = *ctx->prmlst++;

				for ( src = (char *) *ctx->prmlst++; srclen > 0; srclen-- )
					{
					if ( isprint(*src) )
						{ APPEND_C(*src); }
					else	{ APPEND_C('.'); }

					src++;
					}

				break;

				}
			else	return SS$_BADPARAM + 8;

			if ( srclen > (ctx->outsize-j) )
				{
				for ( ; srclen > 0; srclen-- )
					{ APPEND_C(*src); (void) *src++; }
				}
			else	{
				for ( out = ctx->out; srclen > 0; srclen-- )
					out[j++] = *src++;
				}
			}

			break;


		case 'S':
			{	/* Blank filled signed decimal */
			value = (int) *ctx->prmlst++;

			if ( d2 == 'L' )
				{
				}
			else if ( d2 == 'B' )
				{
				value = (value & 255);
				if ( value > 127 ) value |= 0xffffff00;
				}
			else if ( d2 == 'W' )
				{
				value = (value & 0x0ffff);
				if ( value > 0x7fff )
					value |= 0xffff0000;
				}
			else	{ return SS$_BADPARAM + 10; }

			ctx->value = value;
			return	l_rc = s_fmt_signed ( value, ctx );
			}

		case 'U':
		case 'Z':
		case 'O':
		case 'X':
			{
			int	l_base = 10;
			char	l_fill = '0';

			uvalue = *ctx->prmlst++;

			if ( d1 == 'U' )
				l_fill = ' ';
			else if  ( d1 == 'X' )
				l_base = 16;
			else if ( d1 == 'O' )
				l_base = 8;

			if ( d2 == 'B' )
				uvalue = (uvalue & 255);
			else if ( d2 == 'W' )
				uvalue = (uvalue & 0x0ffff);
			else	return SS$_BADPARAM + 12;

			ctx->value = (uvalue == 1) ? 1 : 0;
			return	l_rc = s_fmt_unsigned ( uvalue, l_base, l_fill, ctx );
			}

		case '/':	/* newline */
			ctx->ctloffset = 0;
			APPEND_C ( '\r' )
			APPEND_C ( '\n' )
			break;

		case '_':	/* tab */
			ctx->ctloffset = 0;
			APPEND_C ( '\t' );
			break;

		case '^': 	/* form feed */
			ctx->ctloffset = 0;
			APPEND_C ( '\014' );
			break;

		case '!':	/* exclamation mark */
			ctx->ctloffset = 0;
			APPEND_C ( '!' );
			break;

		case '%':	/* system call, date-time */
			{
			if ( d2 == 'D' || d2 == 'T' )
				return	l_rc = s_fmt_time ( (d2=='D') ? 1 : 0, (struct timespec *) *ctx->prmlst++, ctx );
			else if ( d2 == 'S' )
				{
				if ( (j > 0) && (ctx->value != 1) )
					{
					char	l_prev = ctx->out[j-1];

					if ( _tolower(l_prev) == l_prev )
						{APPEND_C('s');}
					else	{ APPEND_C('S'); }
					}
				}
			else	return SS$_BADPARAM + 14;
			}
			break;

		case '*': 	/* Repeat character */
			return l_rc = s_fmt_pad ( (char) d2, j, ctx );
		default:

			return SS$_BADPARAM+16;
		}

	if ( ctx->width_type )
		return l_rc = s_fmt_pad ( ' ', j, ctx );

	return	ctx->outused = j, STS$K_SUCCESS;
}
/************************************************************************/
static int s_fmt_pad ( char fill, int cur, CTL$_BUF *ctx )
{
char * out;
int j, limit;

	if ( cur >= 0 )
		{
		limit = ctx->width + ctx->outused;

		if ( limit < cur )
			{ cur = ctx->outused; fill = '*'; ctx->width_type =  (-1); }
		}
	else	{
		/* Right justify, fil field to (-cur) spaces */
		limit = ctx->width + ctx->outused + cur;

		if ( limit < ctx->outused )
			{ limit = limit-cur; fill = '*'; ctx->width_type = (-1); }

		cur = ctx->outused;
		}

	if ( limit <= ctx->outsize )
		{
		out = ctx->out;
		ctx->outused = limit;

		for ( j = cur; j < limit; )
			out[j++] = fill;

		return STS$K_SUCCESS;
		}


	ctx->outused = ctx->outsize;

	for ( j = cur; j < ctx->outused; )
		out[j++] = fill;

	return SS$_BUFFEROVF;

}

/************************************************************************/
static int s_fmt_signed ( int value, CTL$_BUF *ctx )
{
int	j, length, status, negative, digit_stack[12];

	/*
	* Test for zero case (algorithm below results in null string on zero).
	* If not zero, normalize negative number (should test for MIN_INT).
	*/
	j = ctx->outused;

	if ( !value )
		{
		if ( ctx->width_type )
			{
			status = s_fmt_pad ( ' ', -1, ctx );

			if ( status != 1 )
				return status;

			j = ctx->outused;

			if ( ctx->width_type == -1 )
				return STS$K_SUCCESS;
			}

		APPEND_C ( '0' );
		return	ctx->outused = j, STS$K_SUCCESS;
		}
	else if ( value < 0 )
		{
		/* In 2's complement format, the negative of the min. integer can
		* not be represented in the same number of bits, check for that case.
		*/
		if ( value == MIN_INT )
			{
			char *str;

			if ( ctx->width_type )
				{
				if ( 1 != (status = s_fmt_pad ( ' ', -MIN_INT_STR_L, ctx )) )
					return status;

				if ( ctx->width_type == -1 )
					return STS$K_SUCCESS;

				j = ctx->outused;
				}

			for ( str = MIN_INT_STR; *str; str++ )
				{APPEND_C ( *str );}

			return	ctx->outused = j, STS$K_SUCCESS;
			}

		negative = 1;
		value = -value;
		}
	else	negative = 0;


	/*
	*  Build stack of successively higher order digits.
	*/
	for (length = 0; value > 0; value = value / 10)
		digit_stack[length++] = value;

	if ( ctx->width_type )
		{
		if ( 1 != (status = s_fmt_pad ( ' ', -(length+negative), ctx )) )
			return status;

		if ( ctx->width_type == -1 )
			return STS$K_SUCCESS;

		j = ctx->outused;
		}

	/*
	* pop stack and make lowest digit an ascii char in output buffer.
	*/
	if ( negative )
		{APPEND_C('-');}

	for ( digit_stack[length] = 0; length > 0; length-- )
		{APPEND_C( (char) ((digit_stack[length - 1] - (digit_stack[length] * 10)) + 48) );}

	return	ctx->outused = j, STS$K_SUCCESS;
}
/************************************************************************/
static int s_fmt_unsigned ( LONG value, int base, char fill, CTL$_BUF *ctx )
{
int j, status, digit_stack[12], length;
const char *ascdigit = "0123456789ABCDEF";

	/*
	* Test for zero case (algorithm below results in null string on zero).
	*/
	j = ctx->outused;

	if ( !value )
		{
		if ( ctx->width_type )
			{
			if ( 1 != (status = s_fmt_pad ( fill, -1, ctx )) )
				return status;

			if ( ctx->width_type == -1 )
				return STS$K_SUCCESS;

			j = ctx->outused;
			}

		APPEND_C ( '0' );
		ctx->outused = j;
		return 1;
		}

	/*
	*  Build stack of successively higher order digits.
	*/
	for (length=0; value > 0; value = value/base)
		digit_stack[length++] = value;

	if ( ctx->width_type )
		{
		if ( 1 != (status = s_fmt_pad ( fill, -(length), ctx )) )
			return status;

		if ( ctx->width_type == -1 )
			return STS$K_SUCCESS;

		j = ctx->outused;
		}


	/*
	* pop stack and make lowest digit an ascii char in output buffer.
	*/
	for ( digit_stack[length] = 0; length > 0; length-- )
		{
		int l_num = digit_stack[length - 1] -(digit_stack[length] * base);

		APPEND_C ( ascdigit[l_num] );
		/* APPEND_C( (char)
		((digit_stack[length-1]-(digit_stack[length]*base)) + 48) ); */
		}

	return	ctx->outused = j, STS$K_SUCCESS;
}






#if __util_DEBUG__

int	main (int argc, char *argv[])
{
int count, bcnt;
char	buf[1024];


	bcnt  = __util$fao("!UL, !UW, !UB", buf, sizeof(buf), 1, 2, 3);

	for (bcnt = 0; bcnt < 32; bcnt++)
		$INSQTAIL(&free_bufs, &_stm_buf_area[bcnt], &count);

	return	1;
}




#if	0
// Factory macros
#undef	$DEFMSG

#define $DEFMSGLIST \
	$DEFMSG(util,  0, S, NORMAL,	"normal successful completion") \
	$DEFMSG(util,  1, W, WARNING,	"unknown warning") \
	$DEFMSG(util,  2, E, ERROR,	"unknown error") \
	$DEFMSG(util,  3, F, FATALERR,	"unknown fatal error") \
	$DEFMSG(util, 13, E, NUMCNVERR,	"numeric conversion error: !SL") \

#define	$DEFMSG(fac, num, sev, nam, txt) #nam,
static const char *__$msgmnemonics[] = {
	$DEFMSGLIST
};
#undef	$DEFMSG

#define $DEFMSG(fac, num, sev,nam, txt) txt,
static const char *__$msgtexts[] = {
	$DEFMSGLIST
};
#undef	$DEFMSG

#define $DEFMSG(fac, num, sev, nam, txt) #define fac##txt num
static const char *__$msgtexts[] = {
	$DEFMSGLIST
};

#define	$DEFMSG(fac, num, sev, nam, txt) fac##__##nam =  STC_CODE_##typ(msg),
typedef enum {
STATCODES
} statcode_t;
#undef STATCODE



//#define	$MSGCOUNT (sizeof(__$##fac##msgmnemonics)/sizeof(__$##fac##msgmnemonics))

#endif



#if	0
OPTS	opts [] = {
		OPTS_NULL
};


#define	IOBUFSZ		4096		/* A size of single buffer			*/
#define	IOBUFCNT	4096		/* A total number of buffers			*/

struct _stm_buf				/* One item of Stream buffer			*/
{
	ENTRY	links;			/* Left, right links, used by queue macros	*/
	int	len;			/* Actual length of data in the buffer		*/
	int	ref;			/* Count of reference				*/
	char buf[IOBUFSZ];		/* Buffer itself				*/
} _stm_buf_area[IOBUFCNT];		/* We declare a static buffers area		*/

QUEUE	free_bufs = QUEUE_INITIALIZER,
	busy_bufs = QUEUE_INITIALIZER;


DWORD WINAPI worker_f2b (void *arg)
{
int	status, count, i;
struct _stm_buf	* sbuf;

	$LOG(STS$K_INFO, "tid :%d running ...", GetCurrentThreadId());

	for (i = 0; i < 10000 * 500; i++)
		{
		if ( !(i %  10000) )
			$LOG(STS$K_INFO, "tid:%d, iteration %d", GetCurrentThreadId(), i);

		if ( !(1 & (status = $REMQHEAD(&free_bufs, &sbuf, &count))) )
			{
			$LOG(STS$K_ERROR, "$REMQHEAD() ->  %d, count = %d", status, count);
			continue;
			}

		if ( !sbuf )
			$LOG(STS$K_ERROR, "sbuf == NULL");


		if ( !(1 & (status = status = $INSQTAIL(&busy_bufs, sbuf, &count))) )
			$LOG(STS$K_ERROR, "$INSQTAIL() ->  %d", status);
		}

	return	status;
}

DWORD WINAPI worker_b2f (void *arg)
{
	int	status, count, i;
	struct _stm_buf	* sbuf;

	$LOG(STS$K_INFO, "tid :%d running ...", GetCurrentThreadId());

	for (i = 0; i < 10000 * 500; i++)
		{
		if ( !(i %  10000) )
			$LOG(STS$K_INFO, "tid:%d, iteration %d", GetCurrentThreadId(), i);

		if ( !(1 & (status = $REMQHEAD(&busy_bufs, &sbuf, &count))) )
			{
			$LOG(STS$K_ERROR, "$REMQHEAD() ->  %d, count = %d", status, count);
			continue;
			}


		if ( !sbuf )
			$LOG(STS$K_ERROR, "sbuf == NULL");


		if ( !(1 & (status = status = $INSQTAIL(&free_bufs, sbuf, &count))) )
			$LOG(STS$K_ERROR, "$INSQTAIL() ->  %d", status);
		}

	return	status;
}



int	main (int argc, char *argv[])
{
int count, i;
HANDLE scan_tid;
char	buf[1024];


	i = __util$fao("!UL, !UW, !UB", buf, sizeof(buf), 1, 2, 3);


	/*
	**
	*/
	i = __util$pattern_match("ch33---0010904935.torrent", "*.torrent");

	i = __util$pattern_match("Hello World!!!", "*llo*");




	__util$getparams(argc, argv, opts);
//	__util$setlogfile("logfile2.log");


	//InitializeSRWLock(&free_bufs.lock);




	for (i = 0; i < IOBUFCNT; i++)
		$INSQTAIL(&free_bufs, &_stm_buf_area[i], &count);



	for (i = 0; i <  32; i++)
		{
		if ( (scan_tid = CreateThread(NULL, 0, worker_f2b, NULL, 0, NULL)) == NULL )
			return $LOG(STS$K_ERROR, "Creation streaming thread was failed :%d", GetLastError());
		}


	for (i = 0; i <  32; i++)
		{
		if ( (scan_tid = CreateThread(NULL, 0, worker_b2f, NULL, 0, NULL)) == NULL )
			return $LOG(STS$K_ERROR, "Creation streaming thread was failed :%d", GetLastError());
		}

	WaitForSingleObject(scan_tid, INFINITE);

	return STS$K_SUCCESS;
}
#endif
#endif









#ifndef	WIN32
	//#pragma GCC diagnostic pop
#endif
