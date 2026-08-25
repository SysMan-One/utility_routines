/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  Exercises __util$setlogfd(): it must point the logging at the
**	given descriptor without opening a file or seizing the standard streams,
**	and reject a negative descriptor.  A utility whose standard output carries
**	data relies on this to divert its $LOG/$TRACE to the standard error while
**	leaving stdout alone - unlike __util$deflog(), which dup2()s over both.
**
**  MODULE:  test_setlogfd.c
**--
*/
#define __MODULE__ "SLFT$"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "test_harness.h"
#include "utility_routines.h"

int main (void)
{
	char	l_tmpl[] = "/tmp/setlogfd_XXXXXX";
	char	l_buf[512];
	int	l_fd;
	ssize_t	l_n;

	printf("== setlogfd regression ==\n");

	/* A negative descriptor is refused and leaves the current sink in place. */
	CHECK( !(1 & __util$setlogfd(-1)), "a negative descriptor must be rejected");

	l_fd = mkstemp(l_tmpl);
	CHECK( l_fd >= 0, "mkstemp must succeed");

	/* Redirect the log to the descriptor and confirm a record lands there. */
	CHECK( 1 & __util$setlogfd(l_fd), "setlogfd(valid) must succeed");

	__util$out("marker %d done", 12345);

	lseek(l_fd, 0, SEEK_SET);
	l_n = read(l_fd, l_buf, sizeof(l_buf) - 1);
	l_buf[(l_n > 0) ? l_n : 0] = '\0';

	CHECK( strstr(l_buf, "marker 12345 done") != NULL,
		"the record must land on the descriptor given to setlogfd");

	/* Restore the default sink before the file goes away. */
	__util$setlogfd(STDOUT_FILENO);
	close(l_fd);
	unlink(l_tmpl);

	TEST_DONE();
}
