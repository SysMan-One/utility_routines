/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  The fast log-record prefix (s_util$prefix). The prefix must stay
**	byte-identical to the historical snprintf of
**	"%02u-%02u-%04u %02u:%02u:%02u.%03u " UTIL$T_PID_FMT: the test parses a
**	live record and re-renders the parsed fields through that very format,
**	then compares the bytes. The per-second cache must give two records of
**	the same second an identical date-time part, and the cached thread id
**	must equal the kernel's.
**
**  MODULE:  test_log_prefix.c
**--
*/
#define _GNU_SOURCE
#define __MODULE__ "LPFX$"

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
#include "test_harness.h"
#include "utility_routines.h"

static int s_read_line (int a_fd, char *a_buf, int a_sz)
{
int	l_n = (int) read (a_fd, a_buf, (size_t) (a_sz - 1));

	a_buf[(l_n > 0) ? l_n : 0] = '\0';
	return	l_n;
}

int main (void)
{
char	l_buf [512], l_ref [64], l_datim [20];
int	l_fd [2], l_n, l_k, l_tid;
unsigned l_d, l_mo, l_y, l_h, l_mi, l_se, l_ms;

	printf("== log prefix regression ==\n");

	CHECK( 0 == pipe(l_fd), "a pipe for the log capture");
	__util$setlogfd (l_fd[1]);

	/* A record's prefix re-rendered through the historical format is byte-identical. */
	__util$log ("TFAC", STS$K_INFO, "hello %d", 42);
	l_n = s_read_line (l_fd[0], l_buf, sizeof(l_buf));
	CHECK( l_n > 31, "a record was written");

	l_k = sscanf (l_buf, "%2u-%2u-%4u %2u:%2u:%2u.%3u %d ",
			&l_d, &l_mo, &l_y, &l_h, &l_mi, &l_se, &l_ms, &l_tid);
	CHECK( l_k == 8, "the prefix parses as date, time, fraction and tid");

	snprintf (l_ref, sizeof(l_ref), "%02u-%02u-%04u %02u:%02u:%02u.%03u %6d ",
			l_d, l_mo, l_y, l_h, l_mi, l_se, l_ms, l_tid);
	CHECK( 0 == memcmp (l_buf, l_ref, strlen(l_ref)),
		"the prefix is byte-identical to the historical snprintf rendering");

	/* The cached thread id equals the kernel's. */
	CHECK( l_tid == (int) syscall (SYS_gettid), "the tid column matches gettid()");

	/* Two records of the same second share the very same date-time bytes; on
	** an (unlucky) second boundary between the two, retry once.
	*/
	for ( l_k = 0; l_k < 3; l_k++ )
		{
		__util$log ("TFAC", STS$K_INFO, "a");
		l_n = s_read_line (l_fd[0], l_buf, sizeof(l_buf));
		memcpy (l_datim, l_buf, 20);

		__util$log ("TFAC", STS$K_INFO, "b");
		l_n = s_read_line (l_fd[0], l_buf, sizeof(l_buf));

		if ( 0 == memcmp (l_datim, l_buf, 20) )
			break;
		}
	CHECK( 0 == memcmp (l_datim, l_buf, 20),
		"two records of one second share the cached date-time part");

	/* The FAO signalling path carries the same prefix shape. */
	__util$putmsg_fao ($DEFFAC(0x7F) | $DEFMSG(1) | $DEFSEV(STS$K_INFO), 0);
	l_n = s_read_line (l_fd[0], l_buf, sizeof(l_buf));
	l_k = sscanf (l_buf, "%2u-%2u-%4u %2u:%2u:%2u.%3u %d ",
			&l_d, &l_mo, &l_y, &l_h, &l_mi, &l_se, &l_ms, &l_tid);
	CHECK( l_k == 8, "the putmsg_fao record carries the same prefix shape");

	__util$setlogfd (STDERR_FILENO);
	TEST_DONE();
}
