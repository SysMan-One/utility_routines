/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  The per-thread cache of the log-record prefix (s_util$prefix, UTIL$TLS). Three threads log one
**	after another (main, then two workers, each joined before the next): every record must carry the thread
**	id of its own writer, so the three tid columns differ; on Linux and Windows the column must equal the
**	thread's own id. Under MinGW GCC, which ignores __declspec(thread), the cache was shared and all the
**	records carried the tid of the first writer (utility_routines V.01-34ECO2).
**
**  MODULE:  test_log_tls.c
**--
*/
#define _GNU_SOURCE
#define __MODULE__ "LTLS$"

#include <string.h>
#include <stdlib.h>
#include "test_harness.h"
#include "utility_routines.h"

#ifdef	_WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#define	$PIPE(fd)	_pipe((fd), 65536, _O_BINARY)
#define	$READ		_read
#define	$CLOSE		_close
static unsigned	s_mytid (void) { return (unsigned) GetCurrentThreadId(); }
#else
#include <unistd.h>
#include <pthread.h>
#ifdef	__linux__
#include <sys/syscall.h>
static unsigned	s_mytid (void) { return (unsigned) syscall(SYS_gettid); }
#else
static unsigned	s_mytid (void) { return 0; }			/* No check of the value, only of the difference */
#endif
#define	$PIPE(fd)	pipe(fd)
#define	$READ		read
#define	$CLOSE		close
#endif

static unsigned	s_expected[3];

static void	s_log (int a_idx)
{
	s_expected[a_idx] = s_mytid();
	__util$log ("TFAC", STS$K_INFO, "writer %d", a_idx);
}

#ifdef	_WIN32
static DWORD WINAPI	s_worker (LPVOID a_arg) { s_log ((int) (intptr_t) a_arg); return 0; }

static void	s_run (int a_idx)
{
HANDLE	l_h = CreateThread (NULL, 0, s_worker, (LPVOID) (intptr_t) a_idx, 0, NULL);

	WaitForSingleObject (l_h, INFINITE);
	CloseHandle (l_h);
}
#else
static void *	s_worker (void *a_arg) { s_log ((int) (intptr_t) a_arg); return NULL; }

static void	s_run (int a_idx)
{
pthread_t	l_t;

	pthread_create (&l_t, NULL, s_worker, (void *) (intptr_t) a_idx);
	pthread_join (l_t, NULL);
}
#endif

int main (void)
{
char	l_buf [4096], *l_line, *l_next;
int	l_fd [2], l_n, l_got = 0, l_idx;
unsigned l_tid [3] = {0}, l_d, l_mo, l_y, l_h, l_mi, l_se, l_ms, l_t;

	printf("== log prefix: per-thread cache ==\n");

	CHECK( 0 == $PIPE(l_fd), "a pipe for the log capture");
	__util$setlogfd (l_fd[1]);

	s_log (0);								/* Fills the cache of the main thread first */
	s_run (1);
	s_run (2);

	__util$setlogfd (2);
	$CLOSE (l_fd[1]);

	l_n = 0;
	while ( (l_got = (int) $READ (l_fd[0], l_buf + l_n, (unsigned) (sizeof(l_buf) - 1 - l_n))) > 0 )
		l_n += l_got;
	l_buf[l_n] = '\0';

	for ( l_line = l_buf, l_got = 0; l_line && *l_line; l_line = l_next )
		{
		if ( (l_next = strchr (l_line, '\n')) )
			*l_next++ = '\0';

		if ( 8 != sscanf (l_line, "%2u-%2u-%4u %2u:%2u:%2u.%3u %u ", &l_d, &l_mo, &l_y, &l_h, &l_mi, &l_se, &l_ms, &l_t) )
			continue;

		if ( !strstr (l_line, "writer ") )
			continue;

		l_idx = atoi (strstr (l_line, "writer ") + 7);

		if ( (l_idx >= 0) && (l_idx < 3) )
			l_tid[l_idx] = l_t, l_got++;
		}

	CHECK( l_got == 3, "three records captured (%d)", l_got);
	CHECK( (l_tid[0] != l_tid[1]) && (l_tid[1] != l_tid[2]) && (l_tid[0] != l_tid[2]),
		"every thread has its own tid in the prefix (%u, %u, %u)", l_tid[0], l_tid[1], l_tid[2]);

	if ( s_expected[0] )
		CHECK( (l_tid[0] == s_expected[0]) && (l_tid[1] == s_expected[1]) && (l_tid[2] == s_expected[2]),
			"the tid columns are the writers' own ids (%u/%u, %u/%u, %u/%u)",
			l_tid[0], s_expected[0], l_tid[1], s_expected[1], l_tid[2], s_expected[2]);

	TEST_DONE();
}
