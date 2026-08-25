/*
**++
**
**  FACILITY:	UTILITY$ROUTINES regression tests
**
**  MODULE:	test_putmsg_fao.c
**
**  ABSTRACT:	Checks that $PUTMSG_FAO reads exactly the parameters it was
**		given, and no more.
**
**  DESCRIPTION: The routine takes its parameters through a variable
**		argument list, so the only thing that can tell it how many
**		there are is the caller.  Reading a fixed sixteen of them
**		is undefined behaviour in itself; worse, it then tells
**		__util$faol() that sixteen were supplied, which defeats
**		that routine's own guard against a control string carrying
**		more directives than there are parameters.  A catalogue
**		entry and a caller that disagree - which is a mistake, but
**		an ordinary one - would then format whatever the stack
**		happened to hold, and an !AZ directive would dereference
**		it.
**
**		The check formats a message whose text carries three
**		directives while passing one parameter.  With the count
**		honoured this is harmless: __util$faol() stops where the
**		parameters stop.  Without it the process reads wild memory,
**		which is a SEGV under a sanitizer and silence without one -
**		hence the first check, which pins the visible result too.
**
**  AUTHOR:	StarLet squad
**
**  CREATION DATE:   3-AUG-2026
**
**--
*/

#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>

#include	"utility_routines.h"
#include	"starlet.h"
#include	"test_harness.h"

#define	TFAC$K_FACNO	0x07F1

#define	TFAC$_ONE	($DEFFAC(TFAC$K_FACNO) | $DEFMSG(1) | $DEFSEV(STS$K_ERROR))
#define	TFAC$_THREE	($DEFFAC(TFAC$K_FACNO) | $DEFMSG(2) | $DEFSEV(STS$K_ERROR))

static	EMSG_RECORD		s_msgs [2];
static	EMSG_RECORD_DESC	s_dsc;

/*
**  Fill one record of the catalogue: the text is an ASCIC, so the length
**  goes ahead of it.
*/
static	void	s_setmsg	(
		EMSG_RECORD *	a_rec,
		int		a_sts,
	const	char *		a_fao
			)
{
size_t	l_len = strlen(a_fao);

	a_rec->sts	= a_sts;
	a_rec->textl	= (unsigned char) l_len;

	memcpy(a_rec->text, a_fao, l_len + 1);
}


/*
**  Leave recognisable rubbish where a routine reading past its parameters
**  would look.  Without this the check passes by luck: a clean stack holds
**  zeroes, and an !AZ directive given a null pointer is handled quietly.
**  The rubbish makes the defect show itself every time instead of now and
**  then.
*/
static	void	s_dirty_the_frame	(void)
{
volatile unsigned long	l_junk [64];

	for ( int i = 0; i < (int) (sizeof(l_junk)/sizeof(l_junk[0])); i++ )
		l_junk[i] = 0x4141414141414141UL + (unsigned long) i;

	if ( !l_junk[0] )
		printf("unreachable\n");
}


int	main	(void)
{
int	l_fd [2];
char	l_buf [512];
ssize_t	l_red;

	s_setmsg(&s_msgs[0], TFAC$_ONE,   "one: !AZ");
	s_setmsg(&s_msgs[1], TFAC$_THREE, "three: !AZ !AZ !AZ");

	s_dsc.msgnr	= 2;
	s_dsc.facno	= TFAC$K_FACNO;
	s_dsc.msgrec	= s_msgs;
	s_dsc.facname	= "TFAC";

	__util$inimsg(&s_dsc);

	/*  The record goes to the log descriptor, so a pipe collects it  */
	CHECK(0 == pipe(l_fd), "pipe() failed");

	__util$setlogfd(l_fd[1]);

	/*  A text and a caller that agree  */
	__util$putmsg_fao(TFAC$_ONE, 1, (fao_prm_t) "parameter");

	memset(l_buf, 0, sizeof(l_buf));
	l_red = read(l_fd[0], l_buf, sizeof(l_buf) - 1);

	CHECK(l_red > 0, "nothing was written for TFAC$_ONE");
	CHECK(NULL != strstr(l_buf, "one: parameter"), "the text of TFAC$_ONE is wrong");

	/*
	**  A text carrying three directives, one parameter given.  The
	**  surplus directives must yield nothing at all - and must not send
	**  the routine reading past the parameters it was handed.
	*/
	s_dirty_the_frame();

	__util$putmsg_fao(TFAC$_THREE, 1, (fao_prm_t) "parameter");

	memset(l_buf, 0, sizeof(l_buf));
	l_red = read(l_fd[0], l_buf, sizeof(l_buf) - 1);

	CHECK(l_red > 0, "nothing was written for TFAC$_THREE");
	CHECK(NULL != strstr(l_buf, "three: parameter"), "the text of TFAC$_THREE is wrong");

	/*  Nothing may follow the one parameter but blanks  */
	CHECK(NULL == strstr(l_buf, "three: parameter parameter"),
			"a parameter was formatted twice");

	/*  The $PUTMSG_FAO() macro must cast each parameter itself - written plain  */
	__util$setlogfd(l_fd[1]);
	$PUTMSG_FAO(TFAC$_ONE, "viamacro");
	memset(l_buf, 0, sizeof(l_buf));
	l_red = read(l_fd[0], l_buf, sizeof(l_buf) - 1);
	CHECK(l_red > 0, "nothing was written via the macro");
	CHECK(NULL != strstr(l_buf, "one: viamacro"),
			"$PUTMSG_FAO must cast the plain parameter for the caller");

	/*
	**  The count is derived from the list, so a text carrying more
	**  directives than there are parameters cannot make the routine read
	**  past them - there is no number left for a caller to get wrong.
	*/
	s_dirty_the_frame();

	$PUTMSG_FAO(TFAC$_THREE, "single");

	memset(l_buf, 0, sizeof(l_buf));
	l_red = read(l_fd[0], l_buf, sizeof(l_buf) - 1);

	CHECK(l_red > 0, "nothing was written for the derived count");
	CHECK(NULL != strstr(l_buf, "three: single"), "the derived count formatted the wrong text");
	CHECK(NULL == strstr(l_buf, "three: single single"), "a parameter was formatted twice");

	/*  And a call with no parameter at all  */
	$PUTMSG_FAO(TFAC$_ONE);

	memset(l_buf, 0, sizeof(l_buf));
	l_red = read(l_fd[0], l_buf, sizeof(l_buf) - 1);

	CHECK(l_red > 0, "nothing was written for a parameterless call");

	/*  And a count of zero must be as harmless as any other  */
	__util$putmsg_fao(TFAC$_ONE, 0);

	memset(l_buf, 0, sizeof(l_buf));
	l_red = read(l_fd[0], l_buf, sizeof(l_buf) - 1);

	CHECK(l_red > 0, "nothing was written for a count of zero");

	/*
	**  One record serves any severity: TFAC$_ONE was registered ERROR, but is
	**  signalled here with WARNING and with INFO. The lookup is by message number,
	**  so the record's text is found each time - not the default NOMSG diagnostic.
	*/
	__util$putmsg_fao(($DEFFAC(TFAC$K_FACNO) | $DEFMSG(1) | $DEFSEV(STS$K_WARN)),
				1, (fao_prm_t) "reused");
	memset(l_buf, 0, sizeof(l_buf));
	l_red = read(l_fd[0], l_buf, sizeof(l_buf) - 1);
	CHECK(NULL != strstr(l_buf, "%TFAC-W-one: reused"),
		"the record must be found and shown WARNING when the status carries it");
	CHECK(NULL == strstr(l_buf, "NOMSG"),
		"a severity change must not fall through to the default diagnostic");

	__util$putmsg_fao(($DEFFAC(TFAC$K_FACNO) | $DEFMSG(1) | $DEFSEV(STS$K_INFO)),
				1, (fao_prm_t) "again");
	memset(l_buf, 0, sizeof(l_buf));
	l_red = read(l_fd[0], l_buf, sizeof(l_buf) - 1);
	CHECK(NULL != strstr(l_buf, "%TFAC-I-one: again"), "the same record must be shown INFO when the status carries it");

	__util$setlogfd(STDERR_FILENO);

	close(l_fd[0]);
	close(l_fd[1]);

	TEST_DONE();
}
