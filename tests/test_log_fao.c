/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  The $LOG_FAO macro and its routines __util$log_fao()/
**	__util$logd_fao(): a $LOG whose body is an FAO control string, with the
**	$PUTMSG_FAO ergonomics - every parameter is cast to fao_prm_t and the
**	count is derived by the macro. Checked: the envelope (timestamp prefix,
**	facility, severity letter), the FAO-formatted body including an address
**	directive, the zero-parameter form, and the returned severity.
**
**  MODULE:  test_log_fao.c
**--
*/
#define _GNU_SOURCE
#define __MODULE__ "LFAO$"
#define __FAC__ "TFAC"

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#ifdef	_WIN32
#include	<io.h>
#include	<fcntl.h>
#define	pipe(fd)	_pipe((fd), 65536, _O_BINARY)	/* the CRT's pipe - the capture reads what the log wrote */
#endif
#include "test_harness.h"
#include "utility_routines.h"
#include "starlet.h"

static int s_read_line (int a_fd, char *a_buf, int a_sz)
{
int	l_n = (int) read (a_fd, a_buf, (size_t) (a_sz - 1));

	a_buf[(l_n > 0) ? l_n : 0] = '\0';
	return	l_n;
}

int main (void)
{
char	l_buf [512];
int	l_fd [2], l_n;
unsigned l_sev;
unsigned char	l_ip4 [4] = { 192,168,1,10 };

	printf("== log_fao regression ==\n");

	CHECK( 0 == pipe(l_fd), "a pipe for the log capture");
	__util$setlogfd (l_fd[1]);

	/* A body with a string and a number, formatted through FAO. */
	l_sev = $LOG_FAO(STS$K_INFO, "if=!AZ rx=!UL", "eth0", 777);
	l_n = s_read_line (l_fd[0], l_buf, sizeof(l_buf));
	CHECK( l_n > 31, "a record was written");
	CHECK( NULL != strstr(l_buf, "%TFAC-"), "the envelope carries the facility");
	CHECK( (NULL != strstr(l_buf, "-I:")) || (NULL != strstr(l_buf, "-i:")),
		"the envelope carries the INFO severity letter");
	CHECK( NULL != strstr(l_buf, "if=eth0 rx=777"), "the FAO body is formatted: '%s'", l_buf);
	CHECK( l_sev == STS$K_INFO, "the macro returns the severity");

#ifndef	NDEBUG
	CHECK( NULL != strstr(l_buf, "[LFAO$\\"), "the debug face carries [module-function-line]");
#endif

	/* An address directive inside the body. */
	$LOG_FAO(STS$K_WARN, "peer=!%I port=!UL", l_ip4, 4020);
	l_n = s_read_line (l_fd[0], l_buf, sizeof(l_buf));
	CHECK( NULL != strstr(l_buf, "peer=192.168.1.10 port=4020"),
		"an address directive works in the body: '%s'", l_buf);

	/* The zero-parameter form must expand and run cleanly. */
	$LOG_FAO(STS$K_ERROR, "no parameters here");
	l_n = s_read_line (l_fd[0], l_buf, sizeof(l_buf));
	CHECK( NULL != strstr(l_buf, "no parameters here"), "the zero-parameter form works");
	CHECK( (NULL != strstr(l_buf, "-E:")) || (NULL != strstr(l_buf, "-e:")),
		"...with the ERROR severity letter");

	__util$setlogfd (STDERR_FILENO);
	TEST_DONE();
}
