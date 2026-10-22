/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  Exercises __util$rotatelogfile(): short of the limit nothing
**	happens; past it the file goes to <file>.1, the older ones shift up to
**	<file>.<keep> and the oldest is dropped; the next record lands in a
**	fresh <file>, through $LOG and through the standard output alike;
**	keep = 0 truncates in place; a keep over 99 is refused; with the log on
**	a descriptor of __util$setlogfd() nothing is ever rotated.
**
**  MODULE:  test_logrotate.c
**
**	22-OCT-2026	RRL	Builds for Windows: mkdtemp() made of _mktemp() + _mkdir(),
**				the files under the current directory there.
**--
*/
#define __MODULE__ "LRT$"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "test_harness.h"
#ifdef	_WIN32
#include <io.h>
#include <direct.h>
/* No mkdtemp() in the Windows C RTL (mkstemp() is there); the files go under the current directory */
static char *mkdtemp (char *a_tmpl) { return ( _mktemp(a_tmpl) && !_mkdir(a_tmpl) ) ? a_tmpl : NULL; }
#define	TEST$TMP	""
#else
#define	TEST$TMP	"/tmp/"
#endif
#include "utility_routines.h"

static char	s_dir [] = TEST$TMP "logrotate_XXXXXX";
static char	s_log [64];

/* Whole file into a buffer, "" when it is not there */
static void	s_slurp (
	const char	*a_path,
		char	*a_buf,
		size_t	 a_sz
		)
{
int	l_fd;
ssize_t	l_n = 0;

	if ( 0 <= (l_fd = open(a_path, O_RDONLY)) )
		{
		l_n = read(l_fd, a_buf, a_sz - 1);
		close(l_fd);
		}

	a_buf[(l_n > 0) ? l_n : 0] = '\0';
}

static int	s_exists (
	const char	*a_path
		)
{
struct stat	l_st;

	return	0 == stat(a_path, &l_st);
}

int main (void)
{
char	l_buf [8192], l_path [96];
int	l_out;

	printf("== logrotate regression ==\n");

	if ( !mkdtemp(s_dir) )
		{
		printf("  FAIL: mkdtemp\n");
		return	1;
		}

	snprintf(s_log, sizeof(s_log), "%s/t.log", s_dir);

	/* __util$deflog() takes stdout over; keep the real one for the tally.
	 * The FAIL lines stay in the stdio buffer until it is put back.
	 */
	setvbuf(stdout, NULL, _IOFBF, 65536);
	fflush(stdout);
	l_out = dup(STDOUT_FILENO);

	/* A descriptor of the caller given by __util$setlogfd() stays open
	 * when __util$deflog() takes the logging over afterwards.
	 */
	{
	char	l_tmpl [] = TEST$TMP "logrotate_fd_XXXXXX";
	int	l_fd = mkstemp(l_tmpl);

	CHECK( (l_fd > STDERR_FILENO) && (1 & __util$setlogfd(l_fd)), "setlogfd(own descriptor)");
	CHECK( 1 & __util$deflog(s_log, NULL), "deflog over a setlogfd descriptor");
	CHECK( 1 == write(l_fd, "x", 1), "the caller's descriptor must still be open");
	close(l_fd);
	unlink(l_tmpl);
	unlink(s_log);
	__util$setlogfd(STDOUT_FILENO);
	}

	/* No file yet: nothing to rotate, and a bad keep is refused anyway	*/
	CHECK( 1 & __util$rotatelogfile(1, 2), "no log file - a no-op");
	CHECK( !(1 & __util$rotatelogfile(1, 100)), "keep 100 must be refused");

	CHECK( 1 & __util$deflog(s_log, NULL), "deflog must open the file");

	__util$out("record one");

	/* Short of the limit */
	CHECK( 1 & __util$rotatelogfile(1 << 20, 2), "short of the limit - success");
	snprintf(l_path, sizeof(l_path), "%s.1", s_log);
	CHECK( !s_exists(l_path), "short of the limit - no t.log.1");

	/* Limit 0 never rotates */
	CHECK( 1 & __util$rotatelogfile(0, 2), "limit 0 - success");
	CHECK( !s_exists(l_path), "limit 0 - no t.log.1");

	/* Past it: t.log -> t.log.1, a fresh t.log */
	CHECK( 1 & __util$rotatelogfile(1, 2), "first rotation");
	s_slurp(l_path, l_buf, sizeof(l_buf));
	CHECK( strstr(l_buf, "record one") != NULL, "t.log.1 holds the old record");

	__util$out("record two");
	write(STDOUT_FILENO, "stdout two\n", 11);
	s_slurp(s_log, l_buf, sizeof(l_buf));
	CHECK( strstr(l_buf, "has been rotated") != NULL, "the new t.log starts with the rotation record");
	CHECK( strstr(l_buf, "record two") != NULL, "$LOG goes to the new t.log");
	CHECK( strstr(l_buf, "stdout two") != NULL, "stdout goes to the new t.log");
	CHECK( strstr(l_buf, "record one") == NULL, "the new t.log does not hold the old record");

	/* Second: t.log.1 -> t.log.2 */
	CHECK( 1 & __util$rotatelogfile(1, 2), "second rotation");
	__util$out("record three");

	snprintf(l_path, sizeof(l_path), "%s.2", s_log);
	s_slurp(l_path, l_buf, sizeof(l_buf));
	CHECK( strstr(l_buf, "record one") != NULL, "t.log.2 holds the oldest record");

	snprintf(l_path, sizeof(l_path), "%s.1", s_log);
	s_slurp(l_path, l_buf, sizeof(l_buf));
	CHECK( strstr(l_buf, "record two") != NULL, "t.log.1 holds the middle record");

	/* Third: the oldest is dropped, never a t.log.3 */
	CHECK( 1 & __util$rotatelogfile(1, 2), "third rotation");

	snprintf(l_path, sizeof(l_path), "%s.3", s_log);
	CHECK( !s_exists(l_path), "keep 2 - no t.log.3");

	snprintf(l_path, sizeof(l_path), "%s.2", s_log);
	s_slurp(l_path, l_buf, sizeof(l_buf));
	CHECK( strstr(l_buf, "record two") != NULL && !strstr(l_buf, "record one"), "t.log.2 now holds the middle record");

	/* keep 0: truncated in place, no new file */
	__util$out("record four");
	CHECK( 1 & __util$rotatelogfile(1, 0), "keep 0 - success");
	__util$out("record five");

	s_slurp(s_log, l_buf, sizeof(l_buf));
	CHECK( strstr(l_buf, "record four") == NULL, "keep 0 - the old records are gone");
	CHECK( strstr(l_buf, "record five") != NULL, "keep 0 - the next record is there");
	CHECK( strncmp(l_buf, "record", 6) && (strstr(l_buf, "truncated") != NULL), "keep 0 - the file starts over with the note");

	/* A descriptor of __util$setlogfd() is not ours to rotate */
	CHECK( 1 & __util$setlogfd(STDERR_FILENO), "setlogfd(stderr)");
	CHECK( 1 & __util$rotatelogfile(1, 2), "after setlogfd - success");

	snprintf(l_path, sizeof(l_path), "%s.3", s_log);
	CHECK( !s_exists(l_path), "after setlogfd - nothing renamed");

	/* Clean up, the real stdout back for the tally */
	__util$setlogfd(STDOUT_FILENO);
	dup2(l_out, STDOUT_FILENO);
	close(l_out);

	for ( int i = 0; i <= 3; i++ )
		{
		if ( i )
			snprintf(l_path, sizeof(l_path), "%s.%d", s_log, i);
		else	snprintf(l_path, sizeof(l_path), "%s", s_log);

		unlink(l_path);
		}

	rmdir(s_dir);

	TEST_DONE();
}
