/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  Guards the logging routines against buffer overflows.  A record
**	longer than the internal/caller buffer must be truncated in bounds and its
**	terminating <LF>/<NUL> written inside the buffer - never one past it.
**	__util$log2buf() writes into the caller's buffer, so it is exercised on a
**	heap block where AddressSanitizer flags a one-byte overrun; __util$out()
**	is exercised for the <LF>-past-the-end overflow of its own stack buffer.
**
**  MODULE:  test_log_overflow.c
**--
*/
#define __MODULE__ "LOGT$"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "test_harness.h"
#include "utility_routines.h"

int main (void)
{
	static char	l_big[8192];
	int		l_sz;

	printf("== log overflow regression ==\n");

	memset(l_big, 'X', sizeof(l_big) - 1);
	l_big[sizeof(l_big) - 1] = '\0';

	/* __util$log2buf() must clamp the reported length to outsz-1 and keep the
	** terminator inside the caller's buffer.  On an ASan heap block a one-byte
	** overrun (__outbuf[outsz]) is caught immediately.
	*/
	for ( l_sz = 32; l_sz <= 4096; l_sz *= 2 )
		{
		char	*l_buf = malloc((size_t) l_sz);
		int	l_olen = -1;

		__util$log2buf(l_buf, l_sz, &l_olen, "TEST", STS$K_INFO, "%s", l_big);

		CHECK( (l_olen >= 0) && (l_olen <= (l_sz - 1)),
			"log2buf(outsz=%d): outlen must be <= outsz-1 (got %d)", l_sz, l_olen);
		CHECK( l_buf[l_olen] == '\0',
			"log2buf(outsz=%d): must NUL-terminate inside the buffer", l_sz);

		free(l_buf);
		}

	/* The fd-writing path: an oversized record must not push the <LF> past the
	** internal buffer.  __util$out() writes to the log fd, so silence fd 1/2 around
	** the calls (restoring them before the CHECKs, so a failure is still reported);
	** ASan guards the overflow itself.
	*/
	{
	unsigned	l_r1, l_r2;
	int		l_saved1 = dup(1), l_saved2 = dup(2);
	int		l_null_fd = open("/dev/null", O_WRONLY);

	fflush(stdout);						/* flush the header before redirecting	*/
	fflush(stderr);
	if ( l_null_fd >= 0 )
		{
		dup2(l_null_fd, 1);
		dup2(l_null_fd, 2);
		}

	l_r1 = __util$out("%s", l_big);				/* oversized record	*/
	l_r2 = __util$out("short line");			/* normal record	*/

	fflush(stdout);
	fflush(stderr);
	if ( l_saved1 >= 0 ) { dup2(l_saved1, 1); close(l_saved1); }
	if ( l_saved2 >= 0 ) { dup2(l_saved2, 2); close(l_saved2); }
	if ( l_null_fd >= 0 ) close(l_null_fd);

	CHECK( 1 & l_r1, "__util$out() with an oversized record must succeed without overflow");
	CHECK( 1 & l_r2, "__util$out() with a normal record must succeed");
	}

	TEST_DONE();
}
