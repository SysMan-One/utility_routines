#define	__MODULE__	"CLI_ROUTINES"
#define	__IDENT__	"X.00-16ECO1"
#define	__REV__		"0.16.1"

/*
**++
**
**  FACILITY:  Command Language Interface (CLI) Routines
**
**  ABSTRACT: A portable API to implement command line parameters parsing, primary syntax checking,
**	and dispatching of processing. This true story is based on the OpenVMS CDU/CLI facilities.
**
**  DESCRIPTION: Command Language Interface (CLI) Routines - an API is supposed to be used to parse and
**	dispatch processing.
**
**	This set of routines help to implement a follows syntax:
**
**	<verb> [p1 p2 ... p8] [/qualifiers[=<value> ...]
**
**	...
**
**  DESIGN ISSUE:
**	{tbs}
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  22-AUG-2018
**
**  MODIFICATION HISTORY:
**
**	15-OCT-2018	RRL	A main part of code has been wrote; split CLI API to .C and .H modules.
**
**	28-JUL-2021	RRL	Project reanimation.
**
**	27-JUL-2026	RRL	X.00-02 : Audit fixes.
**				- __cli$parse() now propagates the parse status instead of always
**				  returning SUCCESS, and releases the half-built context on error;
**				- rewrote and wired up __cli$val_check(): correct UUID check (canonical
**				  8-4-4-4-12 validated via %n length), snprintf() for the device path,
**				  errno reset and trailing-garbage check for numbers, values are taken
**				  as NUL-terminated strings;
**				- wired up __cli$check_keyword() (CLI$K_KWD) and initialised its 'ksel';
**				- checked the __cli$add_item2ctx() status in the qualifier path;
**				- fixed the sub-verb recursion argument (argv + 1), the argc<1 message,
**				  a NULL guard in __cli$cleanup(); removed diagnostic-suppression pragmas
**				  and the resulting -Wparentheses; unified the verb-length macro name.
**
**	28-JUL-2026	RRL	X.00-03 : Renamed the cli$/_cli$ routine prefix to __cli$ (the
**				CLI$K_* constants keep the CLI$ prefix).
**
**	30-JUL-2026	RRL	X.00-04 : Documentation pass - VMS-style **++ headers on every
**				routine; documented the __CLI_DEBUG__ demo; normalised to LF.
**
**	31-JUL-2026	RRL	X.00-05 : Added qualifier defaults (CLI_PQDESC.defval), negatable
**				qualifiers (CLI$M_NEGATABLE, /NO<name>), value lists (CLI$M_LIST,
**				/Q=(a,b,c) returned in turn by __cli$get_value), required
**				parameters/qualifiers (CLI$M_REQUIRED) and __cli$present() to test
**				presence/negation (CLI$M_PRESENT/_NEGATED).
**
**	01-AUG-2026	RRL	X.00-06 : Added value types CLI$K_TIME (absolute/delta time,
**				validated via starlet __util$bintim) and CLI$K_MAC (aa:bb:cc:dd:ee:ff
**				or aa-bb-...).  The module now depends on starlet.
**
**	01-AUG-2026	RRL	X.00-07 : Made cli_routines.h self-contained: CLI_VERB.act_rtn now
**				carries a full prototype (CLI_CTX *, void *) with a forward reference,
**				instead of the __unknown_params placeholder that failed to compile when
**				the header was included alone (GCC 14 -Wdeclaration-missing-parameter-type)
**				and broke under C23. Action routines must be int (CLI_CTX *, void *).
**
**	01-AUG-2026	RRL	X.00-08 : Fixes in the qualifier lookup and the value retrieval:
**				- s_cli$find_qual() dereferenced a NULL verb->quals, which crashed
**				  any verb declaring parameters but no qualifiers;
**				- a zero length token ("-" or "/" alone) was compared by strncasecmp()
**				  with a length of 0 and so matched every qualifier of a table;
**				- an exact, full length qualifier name was reported as ambiguous when
**				  it was also a prefix of a longer one (/LOG vs /LOGFILE), leaving the
**				  shorter qualifier unaddressable; an exact match now wins over an
**				  abbreviation, as it does in the DCL;
**				- __cli$get_value() returned <defval> after a CLI$M_LIST qualifier had
**				  been drained, so a "while (get_value())" loop never terminated;
**				- __cli$check_keyword() had the same exact-versus-prefix defect: a
**				  keyword that is a prefix of another one (INET vs INET6) could not
**				  be spelled out in full, it was always reported as ambiguous;
**				- __cli$parse_params() reported every absent parameter as missing, so
**				  a parameter could never be optional and the CLI$M_REQUIRED flag was
**				  dead code for parameters; a parameter is now optional unless it is
**				  declared CLI$M_REQUIRED;
**				- added <strings.h> for strncasecmp() (musl does not pull it in) and
**				  removed a NULL + 0 pointer arithmetic.
**
**	01-AUG-2026	RRL	X.00-09 : Re-integrated __cli$help() (usage/help generator from the
**				command tables) and the built-in /HELP, -h, --help handling on top of
**				the X.00-08 fixes; help is detected before parameter validation and
**				the parse returns STS$K_WARN so the caller does not dispatch.
**
**	01-AUG-2026	RRL	X.00-11 : Narrowed the built-in help detector, which was too greedy:
**				- it accepted a BARE "help"/"h", with no leading '-' or '/'. Since
**				  __cli$parse() tests argv[0], a caller supplied verb named HELP - the
**				  natural DCL spelling - was answered by the built-in generator and
**				  never reached the verb table. The punctuation is required now;
**				- the same bare form made a PARAMETER VALUE of "help" or "h" print a
**				  usage screen, e.g. "COPY help" for a file so named. Requiring the
**				  punctuation covers this too;
**				- __cli$parse() left *clictx untouched on its two early returns, while
**				  the documented idiom (see tests/) calls __cli$cleanup() on it after a
**				  failed parse; it is zeroed up front now.
**				See tests/test_cli_help.c.
**
**	01-AUG-2026	RRL	X.00-12 : __cli$present() reported the state of the FIRST occurrence
**				of a repeated qualifier, so /HIGHLIGHT /NOHIGHLIGHT was read as
**				affirmative. It now scans the whole item list and returns the LAST
**				occurrence's state, giving DCL "last wins" semantics. See
**				tests/test_cli_features.c.
**
**	 3-AUG-2026	RRL	X.00-13 : The Unix qualifier prefix is now the GNU long form '--' rather
**				than a single '-': __cli$parse_quals() accepts /name or --name, and a
**				lone '-name' is no longer a qualifier. The built-in help detector still
**				accepts the short -h alias.
**
**	 3-AUG-2026	RRL	X.00-14 : __cli$parse_verb() recursed on the matched verb instead of its
**				sub-verb list, so a nested verb was searched in the parent table and
**				SHOW STATISTICS gave 'unrecognized verb statistics'. It now descends
**				into vsel->next. A sub-verb follows its parent immediately (no qualifier
**				between them); qualifiers after the sub-verb are its own. See
**				tests/test_cli_nested.c.
**
**	 7-AUG-2026	RRL	X.00-15 : __cli$parse_params() now stops the positional scan at a token
**				that names a defined qualifier (/name or --name via s_cli$find_qual),
**				leaving it to __cli$parse_quals(). A '/'-path such as /etc/passwd is
**				longer than any qualifier name, so it is not matched and is kept as the
**				parameter's value - a file parameter may hold a leading slash, while a
**				qualifier is no longer swallowed by an omitted optional parameter. See
**				tests/test_cli_filepath.c.
**
**	 7-AUG-2026	RRL	X.00-16 : __cli$parse_quals() reported an unrecognized /qualifier but
**				dropped a bare token in silence. Such a token took neither a parameter
**				(the slots are filled, or a qualifier stopped the positional scan) nor
**				a qualifier, so it is now reported - 'Unrecognized token' - closing the
**				silent-loss hole an optional parameter left open. See test_cli_filepath.c.
**
**	14-AUG-2026	RRL	X.00-16ECO1 : __REV__ read "0.02.0" against __IDENT__ X.00-16; per the
**				Xaa-bbECOc -> aa.bb.c rule it is now "0.16.1" (this very correction
**				being the ECO).
**
**--
*/



#ifdef __cplusplus
    extern "C" {
#define __unknown_params ...
#define __optional_params ...
#else
#define __unknown_params
#define __optional_params ...
#endif


#include	<string.h>
#include	<strings.h>		/* strncasecmp(): musl does not export it via <string.h>	*/
#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<time.h>
#include	<sys/stat.h>
#include	<arpa/inet.h>

/*
* Defines and includes for enable extend trace and logging
*/
#define		__FAC__	"CLI_RTNS"
#define		__TFAC__ __FAC__ ": "		/* Special prefix for $TRACE			*/
#include	"utility_routines.h"
#include	"cli_routines.h"
#include	"starlet.h"		/* __util$bintim(), fao_time_t for CLI$K_TIME */

#define $SHOW_PARM(name, value, format)	$IFTRACE(q_trace, ": " #name " = " format, (value))
#define $SHOW_PTR(var)			$SHOW_PARM(var, var, "%p")
#define $SHOW_STR(var)			$SHOW_PARM(var, (var ? var : "UNDEF(NULL)"), "'%s'")
#define $SHOW_INT(var)			$SHOW_PARM(var, ((int) var), "%d")
#define $SHOW_UINT(var)			$SHOW_PARM(var, ((unsigned) var), "%u")
#define $SHOW_ULL(var)			$SHOW_PARM(var, ((unsigned long long) var), "%llu")
#define $SHOW_UNSIGNED(var)		$SHOW_PARM(var, var, "0x%08x")
#define $SHOW_BOOL(var)			$SHOW_PARM(var, (var ? "ENABLED(TRUE)" : "DISABLED(FALSE)"), "%s");


static	char *__cli$val_type	(
			int	valtype
		)
{
	switch (valtype)
		{
		case	CLI$K_FILE:	return	"FILE (./filename.ext, device:\\path\\filename.ext)";
		case	CLI$K_DATE:	return	"DATE (dd-mm-yyyy[-hh:mm:ss])";
		case	CLI$K_NUM:	return	"DIGIT (decimal, octal, hex)";
		case	CLI$K_IPV4:	return	"IPV4 (aa.bb.cc.dd)";
		case	CLI$K_IPV6:	return	"IPV6";
		case	CLI$K_OPT:	return	"OPTION (no value)";
		case	CLI$K_QSTRING:	return	"ASCII string in double quotes";
		case	CLI$K_UUID:	return	"UUID ( ... )";
		case	CLI$K_KWD:	return	"KEYWORD";
		case	CLI$K_TIME:	return	"TIME (dd-MMM-yyyy hh:mm:ss.cc, or a delta)";
		case	CLI$K_MAC:	return	"MAC (aa:bb:cc:dd:ee:ff)";
		}

	return	"ILLEGAL";
}

/*
 *
 *  DESCRIPTION: Check a input value for the parameter/qualifier corresponding has been declared type

 *
 *  INPUT:
 *	clictx:	CLI-context has been created by __cli$parse()
 *	pqdesc:	Parameter/Qualifier descriptor
 *	val:	an ASCIC string to be checked
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	__cli$check_keyword (char *sts, int len, int opts, CLI_KEYWORD *klist, CLI_KEYWORD **kwd);


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Validate a candidate value string against the type declared in a
**	parameter/qualifier descriptor (CLI$K_FILE, CLI$K_NUM, CLI$K_IPV4, ...).
**	A helper of __cli$parse().
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	pqdesc	The parameter/qualifier descriptor (value type)
**	a_val	The candidate value string
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE.
**
**  RETURN VALUE:
**	STS$K_SUCCESS if the value is valid for its type; error otherwise.
**--
*/
static	int	__cli$val_check	(
		CLI_CTX		*clictx,
		CLI_PQDESC	*pqdesc,
		const char	*a_val
				)
{
char	buf[NAME_MAX], *cp;

	switch (pqdesc->type)
		{
		case	CLI$K_IPV4:
		case	CLI$K_IPV6:
			if ( 1 != inet_pton(pqdesc->type == CLI$K_IPV4 ? AF_INET : AF_INET6, a_val, buf) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Value '%s' cannot be converted, errno=%d", a_val, errno)
					: STS$K_ERROR;
			break;

		case	CLI$K_NUM:
			{
			char	*l_end = NULL;

			errno = 0;
			strtoull(a_val, &l_end, 0);

			if ( (errno == ERANGE) || (l_end == a_val) || (l_end && *l_end) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Value '%s' is not a valid number", a_val)
					: STS$K_ERROR;
			}
			break;

		case	CLI$K_DATE:
			{
			struct tm	l_tm = {0};
			char		l_sep;

			if ( 3 > sscanf(a_val, "%2d-%2d-%4d%c%2d:%2d:%2d",
					&l_tm.tm_mday, &l_tm.tm_mon, &l_tm.tm_year,
					&l_sep,
					&l_tm.tm_hour, &l_tm.tm_min, &l_tm.tm_sec) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Illformed date/time value '%s'", a_val)
					: STS$K_ERROR;
			}
			break;

		case	CLI$K_DEVICE:
			{
			struct stat	l_st = {0};

			/* Accept an absolute "/dev/xxx" as-is, otherwise prefix a bare name */
			if ( a_val == strstr(a_val, "/dev/") )
				cp = (char *) a_val;
			else	snprintf(cp = buf, sizeof(buf), "/dev/%s", a_val);

			if ( stat(cp, &l_st) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "stat(%s), errno=%d", cp, errno)
					: STS$K_ERROR;
			}
			break;

		case	CLI$K_UUID:
			{
			unsigned int	l_u[6];
			int		l_n = -1;

			/* Canonical 8-4-4-4-12 form; the 12-hex node is read as 4 + 8 hex digits.
			 * %n captures the count of consumed characters: a well-formed UUID yields
			 * exactly 6 conversions over exactly 36 characters with nothing trailing.
			 */
			if ( (6 != sscanf(a_val, "%8x-%4x-%4x-%4x-%4x%8x%n",
					&l_u[0], &l_u[1], &l_u[2], &l_u[3], &l_u[4], &l_u[5], &l_n))
				|| (l_n != 36) || (a_val[36] != '\0') )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Illformed UUID value '%s'", a_val)
					: STS$K_ERROR;
			}
			break;

		case	CLI$K_KWD:
			{
			CLI_KEYWORD	*l_kwd;

			if ( !pqdesc->kwd )
				break;			/* no keyword list - nothing to check against */

			return	__cli$check_keyword((char *) a_val, (int) strlen(a_val),
						clictx->opts, pqdesc->kwd, &l_kwd);
			}

		case	CLI$K_TIME:
			{
			fao_time_t	l_tim;

			/* Absolute or delta time, validated by the starlet parser (1 & status). */
			if ( !(1 & __util$bintim(a_val, &l_tim)) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Illformed time value '%s'", a_val)
					: STS$K_ERROR;
			}
			break;

		case	CLI$K_MAC:
			{
			unsigned int	l_o[6];
			int		l_n1 = -1, l_n2 = -1;

			/* Six hex octets separated by ':' or '-'; the whole string must be
			 * consumed (%n) and nothing may trail.
			 */
			if ( !((6 == sscanf(a_val, "%2x:%2x:%2x:%2x:%2x:%2x%n",
					&l_o[0], &l_o[1], &l_o[2], &l_o[3], &l_o[4], &l_o[5], &l_n1) && (a_val[l_n1] == '\0'))
			    || (6 == sscanf(a_val, "%2x-%2x-%2x-%2x-%2x-%2x%n",
					&l_o[0], &l_o[1], &l_o[2], &l_o[3], &l_o[4], &l_o[5], &l_n2) && (a_val[l_n2] == '\0'))) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Illformed MAC address '%s'", a_val)
					: STS$K_ERROR;
			}
			break;

		default:
			/* Types without a specific check (FILE, OPT, QSTRING) are accepted as-is */
			break;
		}


	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Record a matched parameter or qualifier, with its value, in the parse
**	context so that __cli$get_value() can retrieve it later.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	type	Item kind (parameter or qualifier)
**	item	The matched descriptor
**	val	The value string bound to it, or NULL
**	state	Extra item-state bits (e.g. CLI$M_NEGATED); CLI$M_PRESENT is
**		added automatically
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Allocates and links a CLI_ITEM into the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on success; error if memory could not be allocated.
**--
*/
static	int	__cli$add_item2ctx	(
		CLI_CTX		*clictx,
		int		type,
		void		*item,
		char		*val,
		unsigned	state
			)
{
CLI_ITEM	*avp, *avp2;

	/* Allocate memory for new CLI's param/qual value entry */
	if ( !(avp = calloc(1, sizeof(CLI_ITEM))) )
		{
		return	(clictx->opts & CLI$M_OPSIGNAL)
			? $LOG(STS$K_ERROR, "Insufficient memory, errno=%d", errno)
			: STS$K_ERROR;
		}

	/* Store a given item: parameter or qualifier into the context */
	if ( val )
		__util$str2asc (val, &avp->val);

	/* Every parsed item is 'present'; the caller adds _NEGATED where needed */
	avp->state = (unsigned char) (state | CLI$M_PRESENT);

	if ( !type )
		{
		avp->verb = item;

		/* Run over list down to last item */
		for ( avp2 = clictx->vlist; avp2 && avp2->next; avp2 = avp2->next);

		/* Insert new item into the CLI context list */
		if ( avp2 )
			avp2->next = avp;
		else	clictx->vlist = avp;

		return	STS$K_SUCCESS;
		}

	avp->pqdesc = item;

	/* Run over list down to last item */
	for ( avp2 = clictx->avlist; avp2 && avp2->next; avp2 = avp2->next);

	/* Insert new item into the CLI context list */
	if ( avp2 )
		avp2->next = avp;
	else	clictx->avlist = avp;

	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write a human-readable listing of a command-definition table to the log;
**	sub-verb chains are listed recursively with increasing indentation. For
**	documenting or debugging a grammar.
**
**  FORMAL PARAMETERS:
**
**	verbs	The command-definition table
**	level	Initial indentation level; 0 for the top level
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	NONE.
**--
*/
void	__cli$show_verbs	(
	CLI_VERB	*verbs,
		int	level
		)
{
CLI_VERB *verb, *subverb;
CLI_PQDESC *par;
CLI_KEYWORD *kwd;
CLI_PQDESC *qual;
char	spaces [64];

	memset(spaces, ' ', sizeof(spaces));

	for ( verb = verbs; $ASCLEN(&verb->name); verb++)
		{
		if ( !level )
			$LOG(STS$K_INFO, "Show verb '%.*s' : ", $ASC(&verb->name));
		else	$LOG(STS$K_INFO, " %.*s%.*s", level * 2, spaces, $ASC(&verb->name));

		/* Run over command's verbs list ... */
		for ( subverb = verb->next; subverb; subverb = subverb->next)
			__cli$show_verbs(verb->next, ++level);

		for ( par = verb->params; par && par->pn; par++)
			{
			$LOG(STS$K_INFO, "   P%d - '%.*s' (%s)", par->pn, $ASC(&par->name), __cli$val_type (par->type));

			if ( par->kwd )
				{
				for ( kwd = par->kwd; $ASCLEN(&kwd->name); kwd++)
					$LOG(STS$K_INFO, "      %.*s=%#x ", $ASC(&kwd->name), kwd->val);
				}
			}


		for ( qual = verb->quals; qual && $ASCLEN(&qual->name); qual++)
			{
			$LOG(STS$K_INFO, "   /%.*s (%s)", $ASC(&qual->name), __cli$val_type (qual->type) );

			if ( qual->kwd )
				{
				for ( kwd = qual->kwd; $ASCLEN(&kwd->name); kwd++)
					$LOG(STS$K_INFO, "      %.*s=%#x ", $ASC(&kwd->name), kwd->val);
				}
			}

		}
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write the contents of a parse context - the matched verb, parameters and
**	qualifiers with their values - to the log. For debugging.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context to display
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	NONE.
**--
*/
void	__cli$show_ctx	(
		CLI_CTX	*	clictx
		)
{
CLI_ITEM	*avp;
char	spaces [64];
int	splen = 0;

	memset(spaces, ' ', sizeof(spaces));

	$LOG(STS$K_INFO, "CLI-parser context area");


	/* Run over command's verbs list ... */
	for ( splen = 2, avp = clictx->vlist; avp; avp = avp->next, splen += 2)
		$LOG(STS$K_INFO, "%.*s %.*s  ('%.*s')", splen, spaces, $ASC(&avp->verb->name), $ASC(&avp->val));

	for ( avp = clictx->avlist; avp; avp = avp->next)
		{
		if ( avp->type )
			$LOG(STS$K_INFO, "   P%d[0:%d]='%.*s'", avp->pqdesc->pn, $ASCLEN(&avp->val), $ASC(&avp->val));
		else	$LOG(STS$K_INFO, "   /%.*s[0:%d]='%.*s'", $ASC(&avp->pqdesc->name), $ASCLEN(&avp->val), $ASC(&avp->val));
		}
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Print a human-readable usage/help screen derived from the command tables.
**	With <verb> == NULL it lists the available commands; with a container verb
**	it lists the sub-verbs; with a leaf verb it prints a SYNOPSIS line plus the
**	parameters and qualifiers with their types, defaults and flags.  Reached
**	automatically when /HELP, -h or --help appears on the command line.
**
**  FORMAL PARAMETERS:
**
**	verbs	The top-level command table (used when <verb> is NULL)
**	verb	The verb to describe, or NULL for an overview
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination, unprefixed, via __util$out().
**
**  RETURN VALUE:
**	STS$K_SUCCESS.
**--
*/
int	__cli$help	(
	CLI_VERB	*verbs,
	CLI_VERB	*verb
		)
{
CLI_PQDESC	*pq;
CLI_VERB	*v;
char		l_buf[512];
int		l_n;

	/* Overview - just list the top-level verbs. */
	if ( !verb )
		{
		__util$out ("Available commands:");
		for ( v = verbs; v && $ASCLEN(&v->name); v++ )
			__util$out ("    %.*s", $ASC(&v->name));
		__util$out ("Use '<command> /HELP' for details of a command.");
		return	STS$K_SUCCESS;
		}

	/* A container verb - list its sub-verbs. */
	if ( verb->next )
		{
		__util$out ("Usage: %.*s <subcommand> ...", $ASC(&verb->name));
		__util$out ("Subcommands:");
		for ( v = verb->next; v && $ASCLEN(&v->name); v++ )
			__util$out ("    %.*s", $ASC(&v->name));
		return	STS$K_SUCCESS;
		}

	/* SYNOPSIS: the verb, each parameter (required ones bare, optional in []),
	** then a placeholder if the verb takes any qualifiers.
	*/
	l_n = snprintf (l_buf, sizeof(l_buf), "Usage: %.*s", $ASC(&verb->name));

	for ( pq = verb->params; pq && pq->pn && $ASCLEN(&pq->name)
			&& (l_n > 0) && ((size_t) l_n < sizeof(l_buf)); pq++ )
		l_n += snprintf (l_buf + l_n, sizeof(l_buf) - l_n,
			(pq->flag & CLI$M_REQUIRED) ? " %.*s" : " [%.*s]", $ASC(&pq->name));

	if ( verb->quals && $ASCLEN(&verb->quals->name)
			&& (l_n > 0) && ((size_t) l_n < sizeof(l_buf)) )
		snprintf (l_buf + l_n, sizeof(l_buf) - l_n, " [/qualifier ...]");

	__util$out ("%s", l_buf);

	/* Positional parameters. */
	if ( verb->params && $ASCLEN(&verb->params->name) )
		{
		__util$out ("Parameters:");
		for ( pq = verb->params; pq && pq->pn && $ASCLEN(&pq->name); pq++ )
			__util$out ("    P%-2u %-14.*s %s",
				pq->pn, $ASC(&pq->name), __cli$val_type(pq->type));
		}

	/* Qualifiers, with type, flags and default value. */
	if ( verb->quals && $ASCLEN(&verb->quals->name) )
		{
		__util$out ("Qualifiers:");
		for ( pq = verb->quals; pq && $ASCLEN(&pq->name); pq++ )
			{
			char	l_ann[192];

			l_n = snprintf (l_ann, sizeof(l_ann), "%s", __cli$val_type(pq->type));

			if ( (pq->flag & CLI$M_REQUIRED) && (l_n > 0) && ((size_t) l_n < sizeof(l_ann)) )
				l_n += snprintf (l_ann + l_n, sizeof(l_ann) - l_n, ", required");
			if ( (pq->flag & CLI$M_NEGATABLE) && (l_n > 0) && ((size_t) l_n < sizeof(l_ann)) )
				l_n += snprintf (l_ann + l_n, sizeof(l_ann) - l_n, ", /NO<name> accepted");
			if ( (pq->flag & CLI$M_LIST) && (l_n > 0) && ((size_t) l_n < sizeof(l_ann)) )
				l_n += snprintf (l_ann + l_n, sizeof(l_ann) - l_n, ", list");
			if ( $ASCLEN(&pq->defval) && (l_n > 0) && ((size_t) l_n < sizeof(l_ann)) )
				snprintf (l_ann + l_n, sizeof(l_ann) - l_n, ", default=%.*s", $ASC(&pq->defval));

			__util$out ("    /%-14.*s %s", $ASC(&pq->name), l_ann);
			}
		}

	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Match a token against a keyword list, honouring unambiguous-prefix
**	abbreviation, and return the keyword it resolves to.
**
**  FORMAL PARAMETERS:
**
**	sts	The token to match
**	len	Length of the token
**	opts	Matching options
**	klist	The keyword list
**	kwd	Receives the matched keyword
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on a unique match; error if unknown or ambiguous.
**--
*/
static	int	__cli$check_keyword	(
			char	*sts,
			int	 len,
		int		opts,
		CLI_KEYWORD	*klist,
		CLI_KEYWORD **	kwd
		)
{
CLI_KEYWORD	*krun, *ksel = NULL, *kexact = NULL;
int	nmatch = 0, qlog = opts & CLI$M_OPTRACE;

	*kwd = NULL;

	/* An empty token would be matched by strncasecmp() against every keyword */
	if ( !klist || (0 >= len) )
		return	(opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_ERROR, "Empty keyword") : STS$K_ERROR;

	for ( krun = klist; $ASCLEN(&krun->name); krun++)
		{
		if ( len > $ASCLEN(&krun->name) )
			continue;

		$IFTRACE(qlog, "Match [0:%d]='%.*s' against '%.*s'", len, len, sts, $ASC(&krun->name) );

		if ( strncasecmp(sts, $ASCPTR(&krun->name), $MIN(len, $ASCLEN(&krun->name))) )
			continue;

		/* An exact, full length keyword always wins over an abbreviation - otherwise
		 * an INET could not be spelled out as soon as an INET6 is declared.
		 */
		if ( len == $ASCLEN(&krun->name) )
			{
			kexact = krun;
			continue;
			}

		nmatch++;
		ksel = krun;
		}

	if ( kexact )
		{
		*kwd = kexact;
		return	STS$K_SUCCESS;
		}

	if ( 1 < nmatch )
		return	(opts & CLI$M_OPSIGNAL)
			? $LOG(STS$K_FATAL, "Ambiguous input '%.*s'", len, sts)
			: STS$K_FATAL;

	if ( ksel )
		{
		*kwd = ksel;
		return	STS$K_SUCCESS;
		}

	return	(opts & CLI$M_OPSIGNAL)
		? $LOG(STS$K_ERROR, "Illegal or unrecognized keyword '%.*s'", len, sts)
		: STS$K_ERROR;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Parse the /qualifier[=value] portion of a command against a verb's
**	qualifier table, validating each value. A helper of __cli$parse().
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	verb	The matched verb descriptor
**	argc	Remaining argument count
**	argv	Remaining argument vector
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Adds the matched qualifiers to the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on success; error on an unknown qualifier or a bad value.
**--
*/
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Test whether a command-line token is a built-in help request: /HELP,
**	-HELP, --help (any case) or the short -h / --h form.
**
**	The leading '-' or '/' is REQUIRED. A bare "help" is an ordinary token: it
**	may be a verb of the caller's own grammar - HELP is the natural DCL spelling -
**	or the value of a positional parameter, e.g. a file named "help". Neither
**	must be swallowed by the built-in generator.
**
**  FORMAL PARAMETERS:
**
**	a_arg	The raw command-line token
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	Non-zero when the token requests help, zero otherwise.
**--
*/
static	int	s_cli$is_help	(const char *a_arg)
{
const char	*l_p = a_arg;

	if ( !l_p )
		return	0;

	if ( (*l_p != '-') && (*l_p != '/') )			/* The punctuation is mandatory		*/
		return	0;

	l_p++;

	if ( *l_p == '-' )					/* ... and a second '-' of the "--help"	*/
		l_p++;

	if ( ((l_p[0] == 'h') || (l_p[0] == 'H')) && (l_p[1] == '\0') )
		return	1;

	return	(0 == strncasecmp(l_p, "HELP", 4)) && (l_p[4] == '\0');
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Locate a qualifier in a verb's qualifier table by an unambiguous prefix
**	of its name.  A helper of __cli$parse_quals().
**
**  FORMAL PARAMETERS:
**
**	verb	The verb whose qualifier table is searched
**	name	The qualifier name from the command line
**	len	Length of <name>
**	reqflag	If non-zero, only qualifiers whose flag has this bit set are
**		considered (used to restrict a /NO<name> search to CLI$M_NEGATABLE)
**	ambig	Receives 1 when the prefix is ambiguous, 0 otherwise
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	Address of the single matching descriptor, or NULL if none/ambiguous.
**--
*/
static	CLI_PQDESC *	s_cli$find_qual	(
	CLI_VERB	*verb,
	const char	*name,
		int	len,
	unsigned	reqflag,
		int	*ambig
			)
{
CLI_PQDESC	*qrun, *qsel = NULL, *qexact = NULL;
int		nmatch = 0;

	*ambig = 0;

	/*
	 * A verb may declare parameters and no qualifiers at all, and an empty token
	 * ("-" or "/" alone) must not be taken for a zero length prefix - strncasecmp()
	 * with a length of 0 succeeds against every entry of the table.
	 */
	if ( !verb->quals || (0 >= len) )
		return	NULL;

	for ( qrun = verb->quals; $ASCLEN(&qrun->name); qrun++ )
		{
		if ( reqflag && !(qrun->flag & reqflag) )
			continue;			/* wrong class (e.g. not negatable)	*/

		if ( (unsigned) len > $ASCLEN(&qrun->name) )
			continue;			/* longer than the name - not a prefix	*/

		if ( strncasecmp(name, $ASCPTR(&qrun->name), len) )
			continue;

		/*
		 * An exact, full length name always wins over an abbreviation - otherwise
		 * /LOG becomes unaddressable as soon as a /LOGFILE is declared. The scan is
		 * not cut short here: an exact entry may follow the abbreviated ones.
		 */
		if ( (unsigned) len == $ASCLEN(&qrun->name) )
			{
			qexact = qrun;
			continue;
			}

		nmatch++;
		qsel = qrun;
		}

	if ( qexact )
		return	qexact;

	if ( 1 < nmatch )				/* two or more abbreviated matches	*/
		{
		*ambig = 1;
		return	NULL;
		}

	return	qsel;
}

static	int	__cli$parse_quals(
	CLI_CTX	*	clictx,
	CLI_VERB	*verb,
		int	argc,
		char ** argv
			)
{
CLI_PQDESC	*qsel;
int		status, len, i, ambig, negated, qlog = clictx->opts & CLI$M_OPTRACE;
char		*aptr, *vptr;

	/*
	 *  Run over arguments from the command line
	 */
	for ( i = 0; i < argc; i++ )
		{
		aptr = argv[i];

		if ( *aptr == '/' )
			aptr++;					/* VMS form:  /name		*/
		else if ( (aptr[0] == '-') && (aptr[1] == '-') )
			aptr += 2;				/* GNU long form:  --name	*/
		else	/*
			 * A bare token here took neither a parameter nor a qualifier -
			 * the parameters are already filled (or a qualifier stopped the
			 * scan) and this is not a qualifier. Report it rather than drop it
			 * silently: it is a surplus parameter, or a qualifier missing its
			 * '/' or '--'. Symmetric with the unrecognized-qualifier case below.
			 */
			return	(clictx->opts & CLI$M_OPSIGNAL)
				? $LOG(STS$K_FATAL, "Unrecognized token '%s'", aptr)
				: STS$K_FATAL;

		/* Is there a '=' and a value ? */
		if ( (vptr = strchr(aptr, '=')) )
			len = vptr - aptr;
		else	len = strnlen(aptr, ASC$K_SZ);

		/*
		 *  Affirmative match first; if it fails and the token starts with
		 *  'NO', retry the remainder against the negatable qualifiers only.
		 */
		negated = 0;
		qsel = s_cli$find_qual (verb, aptr, len, 0, &ambig);

		if ( !qsel && !ambig && (len > 2) && !strncasecmp(aptr, "NO", 2) )
			{
			qsel = s_cli$find_qual (verb, aptr + 2, len - 2, CLI$M_NEGATABLE, &ambig);
			negated = (qsel != NULL);
			}

		if ( ambig )
			return	(clictx->opts & CLI$M_OPSIGNAL)
				? $LOG(STS$K_FATAL, "Ambiguous qualifier '%.*s'", len, aptr)
				: STS$K_FATAL;

		if ( !qsel )
			return	(clictx->opts & CLI$M_OPSIGNAL)
				? $LOG(STS$K_FATAL, "Unrecognized qualifier '%.*s'", len, aptr)
				: STS$K_FATAL;

		if ( vptr )				/* step over the '=', a NULL + 0 is an UB	*/
			vptr++;
		$IFTRACE(qlog, "%.*s%s='%s'", $ASC(&qsel->name), negated ? " (negated)" : "", vptr ? vptr : "");

		/*
		 *  A negated qualifier carries no value.
		 */
		if ( negated )
			{
			if ( !(1 & (status = __cli$add_item2ctx (clictx, CLI$K_QUAL, qsel, NULL, CLI$M_NEGATED))) )
				return	status;
			continue;
			}

		/*
		 *  A list qualifier: split "(a,b,c)" or "a,b,c" and record one item
		 *  per element; __cli$get_value() then returns them in turn.
		 */
		if ( (qsel->flag & CLI$M_LIST) && vptr && *vptr )
			{
			char	*p = vptr, l_elem[ASC$K_SZ + 1];
			int	l_n;

			if ( *p == '(' )
				p++;

			while ( *p && (*p != ')') )
				{
				for ( l_n = 0; *p && (*p != ',') && (*p != ')'); p++ )
					if ( l_n < ASC$K_SZ )
						l_elem[l_n++] = *p;
				l_elem[l_n] = '\0';

				if ( l_n )
					{
					if ( !(1 & (status = __cli$val_check(clictx, qsel, l_elem))) )
						return	status;

					if ( !(1 & (status = __cli$add_item2ctx (clictx, CLI$K_QUAL, qsel, l_elem, 0))) )
						return	status;
					}

				if ( *p == ',' )
					p++;
				}

			continue;
			}

		/*
		 *  Ordinary single-valued qualifier.
		 */
		if ( vptr && *vptr )
			if ( !(1 & (status = __cli$val_check(clictx, qsel, vptr))) )
				return	status;

		if ( !(1 & (status = __cli$add_item2ctx (clictx, CLI$K_QUAL, qsel, vptr, 0))) )
			return	status;
		}

	/*
	 *  Enforce required parameters and qualifiers.
	 */
	for ( CLI_PQDESC *r = verb->quals; r && $ASCLEN(&r->name); r++ )
		if ( (r->flag & CLI$M_REQUIRED) && !(1 & __cli$present(clictx, r, NULL)) )
			return	(clictx->opts & CLI$M_OPSIGNAL)
				? $LOG(STS$K_FATAL, "Required qualifier /%.*s is missing", $ASC(&r->name))
				: STS$K_FATAL;

	for ( CLI_PQDESC *r = verb->params; r && r->pn && $ASCLEN(&r->name); r++ )
		if ( (r->flag & CLI$M_REQUIRED) && !(1 & __cli$present(clictx, r, NULL)) )
			return	(clictx->opts & CLI$M_OPSIGNAL)
				? $LOG(STS$K_FATAL, "Required parameter %.*s is missing", $ASC(&r->name))
				: STS$K_FATAL;

	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Bind the positional parameters P1..P8 of a command to a verb's parameter
**	table, validating each value. A helper of __cli$parse().
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	verb	The matched verb descriptor
**	argc	Remaining argument count
**	argv	Remaining argument vector
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Adds the matched parameters to the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on success; error on too few arguments or a bad value.
**--
*/
static	int	__cli$parse_params(
	CLI_CTX	*	clictx,
	CLI_VERB	*verb,
		int	argc,
		char ** argv
			)
{
CLI_PQDESC	*param;
int		status, pi, qlog = clictx->opts & CLI$M_OPTRACE;

	/*
	 * Run firstly over parameters list and extract values
	 */
	for ( pi = 0, param = verb->params; param && (pi < argc) && param->pn; param++, pi++ )
		{
		/*
		 * A token that names a defined qualifier is not taken for a parameter's
		 * value: the positional scan stops and the token is left to
		 * __cli$parse_quals(). So an optional parameter may be omitted in favour of
		 * a qualifier - edit /csv - without the qualifier landing in the parameter.
		 * A '/'-token whose name is NOT a qualifier - a path such as /etc/passwd,
		 * longer than any qualifier name and so never a prefix of one - is not
		 * matched, and is taken as the parameter's value.
		 */
		if ( argv[pi] )
			{
			char	*l_qn = NULL;
			int	l_amb = 0;

			if ( argv[pi][0] == '/' )
				l_qn = argv[pi] + 1;			/* /name			*/
			else if ( (argv[pi][0] == '-') && (argv[pi][1] == '-') )
				l_qn = argv[pi] + 2;			/* --name			*/

			if ( l_qn )
				{
				char	*l_eq  = strchr(l_qn, '=');
				int	l_qlen = l_eq ? (int) (l_eq - l_qn) : (int) strnlen(l_qn, ASC$K_SZ);

				if ( s_cli$find_qual(verb, l_qn, l_qlen, 0, &l_amb) || l_amb )
					break;
				}
			}

		$IFTRACE(qlog, "P%d(%.*s)='%s'", param->pn, $ASC(&param->name), argv[pi] );

		/* Validate the value against the parameter's declared type */
		if ( argv[pi] && *argv[pi] )
			if ( !(1 & (status = __cli$val_check(clictx, param, argv[pi]))) )
				return	status;

		/* Put parameter's value into the CLI context */
		if ( !(1 & (status = __cli$add_item2ctx (clictx, param->pn, param, argv[pi], 0))) )
			return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Error inserting P%d - %.*s into CLI context area", param->pn, $ASC(&param->name)) : STS$K_FATAL;
		}

	/*
	 * Did we get all the mandatory parameters ? A parameter is optional unless it
	 * carries the CLI$M_REQUIRED - otherwise the flag would be a dead code, every
	 * absent parameter having been reported as missing anyway. The trailing ones
	 * are re-checked by the CLI$M_REQUIRED loop of the __cli$parse_quals().
	 */
	if ( param && param->pn && (param->flag & CLI$M_REQUIRED) )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Missing P%d - %.*s !", param->pn, $ASC(&param->name)) : STS$K_FATAL;

	/* Now we can extract qualifiers ... */
	status = __cli$parse_quals(clictx, verb, argc - pi, argv + pi);

	return	status;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Match the leading token of a command against a verb table (honouring
**	abbreviation) and descend into any sub-verb chain. A helper of __cli$parse().
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	verbs	The verb table
**	argc	Argument count
**	argv	Argument vector
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Records the matched verb in the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on a match; error on an unknown or ambiguous verb.
**--
*/
static	int	__cli$parse_verb	(
	CLI_CTX		*clictx,
	CLI_VERB *	verbs,
		int	argc,
		char ** argv
			)
{
CLI_VERB	*vrun, *vsel;
int		status, len, qlog = clictx->opts & CLI$M_OPTRACE;
char		*pverb;

	$IFTRACE(qlog, "argc=%d", argc);

	/*
	 * Sanity check for input arguments ...
	 */
	if ( argc < 1 )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Too few arguments") : STS$K_FATAL;


	pverb = argv[0];

	/*
	 * First at all we need to match  argv[1] in the verbs list
	 */
	len = strnlen(pverb, CLI$SSZ_MAXVERB);

	$IFTRACE(qlog, "argv[1]='%s'->[0:%d]='%.*s'", pverb, len, len, pverb);

	for (vrun = verbs, vsel = NULL; $ASCLEN(&vrun->name); vrun++)
		{
		if ( len > $ASCLEN(&vrun->name) )
			continue;

		$IFTRACE(qlog, "Matching '%.*s' vs '%.*s' ... ", len, pverb, $ASC(&vrun->name));

		/*
		 * Match verb from command line against given verbs table,
		 * we are comparing at minimal length, but we must checks for ambiguous verb's definitions like:
		 *
		 * SET will match verbs: SET & SETUP
		 * DEL will match: DELETE & DELIVERY
		 *
		 */
		if ( !strncasecmp(pverb, $ASCPTR(&vrun->name), $MIN(len, $ASCLEN(&vrun->name))) )
			{
			$IFTRACE(qlog, "Matched on length=%d '%.*s' := '%.*s' !", $MIN(len, $ASCLEN(&vrun->name)), len, pverb, $ASC(&vrun->name));

			/* Check that there is not previous matches */
			if ( vsel )
				{
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_FATAL, "Ambiguous input '%.*s' (matched to : '%.*s', '%.*s')", len, pverb, $ASC(&vrun->name), $ASC(&vsel->name))
					: STS$K_FATAL;
				}

			/* Safe matched verb for future checks */
			vsel = vrun;
			}
		}

	/* Found something ?*/
	if ( !vsel )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Unrecognized command verb '%.*s'", len, pverb) : STS$K_FATAL;

	/*
	 * Ok, we has got in 'vsel' a legal verb, so
	 * so we will now matching arguments from 'argv' against
	 * the verb's parameters and qualifiers list
	 */
	/* Insert new item into the CLI context list */
	__cli$add_item2ctx (clictx, 0, vsel, pverb, 0);

	/* Built-in help: if any following token requests it, show this verb's
	** usage and stop before parameter/qualifier validation.
	*/
	for ( int l_i = 1; l_i < argc; l_i++ )
		if ( s_cli$is_help(argv[l_i]) )
			{
			__cli$help (NULL, vsel);
			return	STS$K_WARN;
			}

	/* Is there a next subverb ? It follows the parent verb immediately -
	** no qualifier stands between them - so argv[1] is the sub-verb, looked
	** up in the parent's sub-verb list vsel->next.
	*/
	if ( vsel->next )
		status = __cli$parse_verb	(clictx, vsel->next, argc - 1, argv + 1);
	else	{
		if ( ( !vsel->params) && (!vsel->quals) )
			return	STS$K_SUCCESS;

		status = __cli$parse_params(clictx, vsel, argc - 1, argv + 1);
		}

	return	status;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Match the program arguments against a command-definition table, validating
**	the verb, positional parameters and qualifiers, and build a context that
**	records the result. The context is consumed by __cli$dispatch() and
**	__cli$get_value() and must be released with __cli$cleanup().
**
**  FORMAL PARAMETERS:
**
**	verbs	The command-definition table
**	opts	Processing options (CLI$M_OPTRACE, CLI$M_OPSIGNAL)
**	argc	Argument count
**	argv	Argument vector
**	clictx	Receives the address of a newly allocated parse context
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Allocates the parse context (released and set to NULL on failure).
**
**  RETURN VALUE:
**	STS$K_SUCCESS on a clean parse; error on a bad value/verb/qualifier or an
**	allocation failure.
**--
*/
int	__cli$parse	(
	CLI_VERB *	verbs,
	int		opts,
		int	argc,
		char ** argv,
		void **	clictx
			)
{
int	status, qlog = opts & CLI$M_OPTRACE;
CLI_CTX	*ctx;

	$IFTRACE(qlog, "argc=%d, opts=%#x", argc, opts);

	/*
	 * The context is zeroed FIRST: a caller is expected to __cli$cleanup() it after a
	 * failed parse, so it must never be left holding an indeterminate value - the two
	 * early returns below used to leave it untouched.
	 */
	*clictx = NULL;

	/*
	 * Sanity check for input arguments ...
	 */
	if ( argc < 1 )
		return	(opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Too few arguments") : STS$K_FATAL;

	/* Create CLI-context area */
	if ( !(*clictx = calloc(1, sizeof(CLI_CTX))) )
		return	(opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Cannot allocate memory, errno=%d", errno) : STS$K_FATAL;
	ctx = *clictx;
	ctx->opts = opts;


	/* A bare help request (e.g. "program /HELP") lists all commands */
	if ( s_cli$is_help(argv[0]) )
		{
		__cli$help (verbs, NULL);
		status = STS$K_WARN;
		}
	else	status = __cli$parse_verb(*clictx, verbs, argc, argv);

	/* Propagate a parsing error to the caller and release the half-built context */
	if ( !(1 & status) )
		{
		__cli$cleanup(ctx);
		*clictx = NULL;
		}

	return	status;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Retrieve the value string bound to a parameter or qualifier descriptor in
**	a parse context. For a CLI$M_LIST qualifier, successive calls return
**	successive values and then report absence. When the item is not present
**	and the descriptor carries a non-empty <defval>, that default is returned.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	pq	The parameter/qualifier descriptor whose value is wanted
**	val	Receives the value as an ASCIC string
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE.
**
**  RETURN VALUE:
**	STS$K_SUCCESS if a value was returned; STS$K_WARN if it was not present.
**--
*/
int	__cli$get_value	(
	CLI_CTX		*clictx,
	CLI_PQDESC	*pq,
		ASC	*val
			)
{
CLI_ITEM *item;
int	 seen = 0;

	/* Sanity check */
	if ( !clictx )
		return	$LOG(STS$K_FATAL, "CLI-context is empty");

	if ( !pq )
		return	$LOG(STS$K_FATAL, "Illegal parameter/qualifier definition");

	for ( item = clictx->avlist; item; item = item->next)
		{
		if ( pq != item->pqdesc )
			continue;

		seen = 1;			/* the item IS present on the command line	*/

		/* For a list qualifier, hand back successive elements: skip the ones
		** already returned and mark the one we return now.
		*/
		if ( (pq->flag & CLI$M_LIST) && (item->state & CLI$M_CONSUMED) )
			continue;

		if ( pq->flag & CLI$M_LIST )
			item->state |= CLI$M_CONSUMED;

		/* Do we need to return a value to the caller ? */
		if ( val )
			{
			*val = item->val;

			if ( !($ASCLEN(val)) )
				return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_WARN, "Zero length value") : STS$K_WARN;
			}

		return	STS$K_SUCCESS;
		}

	/*
	 * Absent: fall back to the declared default. A CLI$M_LIST qualifier which HAS
	 * been given must report exhaustion instead, otherwise a drain loop of the
	 * "while ( 1 & __cli$get_value(...) )" shape would never terminate.
	 */
	if ( val && $ASCLEN(&pq->defval) && !seen )
		{
		*val = pq->defval;
		return	STS$K_SUCCESS;
		}

	return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_ERROR, "No parameter/qualifier ('%.*s') is present in command line",
						       $ASC(&pq->name)) : STS$K_ERROR;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Report whether a parameter or qualifier was supplied on the command line,
**	and in what form.  The analogue of the OpenVMS CLI$PRESENT service.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	pq	The parameter/qualifier descriptor to test
**	state	Optional; receives the item state - CLI$M_PRESENT, plus
**		CLI$M_NEGATED when the qualifier was given as /NO<name>.  Set to
**		zero when the item is absent.  When a qualifier is repeated - most
**		usefully /HIGHLIGHT then /NOHIGHLIGHT - the LAST occurrence on the
**		command line wins, as it does in the DCL.
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE
**
**  RETURN VALUE:
**	STS$K_SUCCESS if present (in any form); STS$K_WARN if absent.
**--
*/
int	__cli$present	(
	CLI_CTX		*clictx,
	CLI_PQDESC	*pq,
	unsigned	*state
		)
{
CLI_ITEM	*item, *l_found = NULL;

	if ( state )
		*state = 0;

	if ( !clictx )
		return	$LOG(STS$K_FATAL, "CLI-context is empty");

	if ( !pq )
		return	$LOG(STS$K_FATAL, "Illegal parameter/qualifier definition");

	/*
	 * Scan the whole list and keep the LAST matching item: when a qualifier is
	 * repeated - most usefully /HIGHLIGHT then /NOHIGHLIGHT - the final occurrence
	 * on the command line decides the reported state, as it does in the DCL.
	 */
	for ( item = clictx->avlist; item; item = item->next )
		if ( item->pqdesc == pq )
			l_found = item;

	if ( l_found )
		{
		if ( state )
			*state = l_found->state;

		return	STS$K_SUCCESS;		/* present (state tells if negated)	*/
		}

	return	STS$K_WARN;				/* absent				*/
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Free all memory owned by a parse context. After the call the context
**	pointer must not be used.
**
**  FORMAL PARAMETERS:
**
**	clictx	The context to release
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Frees the context and its items.
**
**  RETURN VALUE:
**	STS$K_SUCCESS.
**--
*/
int	__cli$cleanup	(
		CLI_CTX	*clictx
		)
{

CLI_ITEM	*avp, *avp2;

	if ( !clictx )
		return	STS$K_SUCCESS;

	/* Run over verb's items list and free has been alocated memory ...*/
	for (avp = clictx->vlist; avp; )
		{
		avp2 = avp;
		avp = avp->next;
		free(avp2);
		}

	/* Run over vlaue's items list and free has been alocated memory ...*/
	for (avp = clictx->avlist; avp; )
		{
		avp2 = avp;
		avp = avp->next;
		free(avp2);
		}

	/* Release CLI-context area */
	free(clictx);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Invoke the action routine registered in the matched verb, passing the
**	context and the verb's act_arg. Within the action, values are retrieved
**	with __cli$get_value().
**
**  FORMAL PARAMETERS:
**
**	clictx	A context produced by __cli$parse()
**
**  IMPLICIT INPUTS/OUTPUTS:
**	NONE.
**
**  RETURN VALUE:
**	Whatever condition value the action routine returns.
**--
*/
int	__cli$dispatch	(
		CLI_CTX	*clictx
			)
{
CLI_ITEM	*item;
CLI_VERB	*verb;

	/* Run over verb's items to last element */
	for ( item = clictx->vlist; item && item->next; item = item->next);

	if ( !item )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "No verb's item has been found in CLI-context") : STS$K_FATAL;

	if ( !(verb = item->verb)  )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "No verb has been found in CLI-context") : STS$K_FATAL;

	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "Action routine=%#x, argument=%#x", verb->act_rtn, verb->act_arg);

	/* Hand control to the action routine registered for the matched verb */
	if ( verb->act_rtn )
		return	verb->act_rtn(clictx, verb->act_arg);

	return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_WARN, "No action routine has been defined") : STS$K_WARN;

}




#ifdef	__CLI_DEBUG__

/* Forward declarations for the objects referenced before their definition below */
CLI_KEYWORD	diff_log_opts [];
int		sort_action	(CLI_CTX *clictx, void *arg);

#include	<stdint.h>

/* The definition tables below use the idiomatic {$ASCINI(..)} / {0} aggregate initialisers;
 * -Wmissing-braces is a benign pedantic complaint about them and is scoped to this example only.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"

#include	<stdio.h>
#include	<errno.h>


/*
	SHOW	VOLUME	<p1-volume_name>/UUID=<uuid> /FULL
		USER	<p1-user-name>  /FULL /GROUP=<user_group_name>
		VM	<p1-VM-name>	/FULL /GROUP=<vm_group_name>

	DIFF	<p1-file-name> <p2-file-name> /BLOCK=(START=<lbn>, END=<lbn>, COUNT=<lbn>)
			/IGNORE=(ERROR) /LOGGING=(FULL, TRACE, ERROR)

	SORT	<p1-file-name>
		/MERGE=(file-spec[,file-spec]))
		/KEY0=(<key-type>, <start-pos>[, <key-size>])
		...
		/KEY8=(<key-type>, <start-pos>[, <key-size>])
			key-type	:= STRING, BYTE, WORD, LONG, QUAD,
			start-post	:= <digit>
			key-size	:= <digit>

		/ORDER=(ASCENDING | DESCENDING)
		/TRACE
		/OUTPUT=<file-spec>
*/

enum	{
	SORT$K_STRING = 0,
	SORT$K_BYTE,
	SORT$K_WORD,
	SORT$K_LONG,
	SORT$K_QUAD
};


enum	{
	SORT$K_ASCENDING = 0,
	SORT$K_DESCENDING,
	SORT$K_TRACE
};


CLI_PQDESC	sort_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_FILE, .name = {$ASCINI("Input file")} },
			{0}};

CLI_KEYWORD	sort_key_types[] = {
			{ .name = {$ASCINI("STRING")},	.val = SORT$K_STRING},
			{ .name = {$ASCINI("BYTE")},	.val = SORT$K_BYTE},
			{ .name = {$ASCINI("WORD")},	.val = SORT$K_WORD},
			{ .name = {$ASCINI("LONG")},	.val = SORT$K_LONG},
			{ .name = {$ASCINI("QUAD")},	.val = SORT$K_QUAD},
			{0}};

CLI_KEYWORD	sort_orders [] = {
			{ .name = {$ASCINI("ASCENDING")}, .val = SORT$K_ASCENDING},
			{ .name = {$ASCINI("DESCENDING")}, .val = SORT$K_DESCENDING},
			{0}};

CLI_KEYWORD	sort_key_opts [] = {
			{ .name = {$ASCINI("")}, .val = SORT$K_ASCENDING},
			{ .name = {$ASCINI("DESCENDING")}, .val = SORT$K_DESCENDING},
			{0}};




CLI_PQDESC	sort_quals [] = {
			{ .name = {$ASCINI("ORDER")}, .type = CLI$K_KWD, .kwd = sort_orders},
			{ .name = {$ASCINI("START")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("END")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("COUNT")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("IGNORE")},	.type = CLI$K_OPT},
			{ .name = {$ASCINI("LOGGING")}, .type = CLI$K_KWD, .kwd = diff_log_opts},
			{0}};


enum	{
	SHOW$K_VOLUME = 1,
	SHOW$K_USER,
	SHOW$K_VM
};


enum	{
	DIFF$K_FULL = 1 << 0,
	DIFF$K_TRACE = 1 << 1,
	DIFF$K_ERROR = 1 << 2,
};

CLI_KEYWORD	diff_log_opts[] = {
			{ .name = {$ASCINI("FULL")},	.val = DIFF$K_FULL},
			{ .name = {$ASCINI("TRACE")},	.val = DIFF$K_TRACE},
			{ .name = {$ASCINI("ERROR")},	.val = DIFF$K_ERROR},
			{0}},

		show_what_opts [] = {
			{ .name = {$ASCINI("VOLUME")},	.val = SHOW$K_VOLUME},
			{ .name = {$ASCINI("USER")},	.val = SHOW$K_USER},
			{ .name = {$ASCINI("VM")},	.val = SHOW$K_VM},
			{0}};

CLI_PQDESC	diff_quals [] = {
			{ .name = {$ASCINI("START")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("END")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("COUNT")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("IGNORE")},	.type = CLI$K_OPT},
			{ .name = {$ASCINI("LOGGING")}, .type = CLI$K_KWD, .kwd = diff_log_opts},
			{0}},

		show_volume_quals [] = {
			{ .name = {$ASCINI("UUID")},	CLI$K_UUID},
			{ .name = {$ASCINI("FULL")},	CLI$K_OPT},
			{0}},
		show_user_quals [] = {
			{ .name = {$ASCINI("GROUP")},	CLI$K_QSTRING},
			{ .name = {$ASCINI("FULL")},	CLI$K_OPT},
			{0}},
		show_vm_quals [] = {
			{ .name = {$ASCINI("GROUP")},	CLI$K_QSTRING},
			{ .name = {$ASCINI("FULL")},	CLI$K_OPT},
			{0}};

CLI_PQDESC	diff_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_FILE, .name = {$ASCINI("Input file 1")} },
			{.pn = CLI$K_P2, .type = CLI$K_FILE, .name = {$ASCINI("Input file 2")} },
			{0}},

		show_volume_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_DEVICE, .name = {$ASCINI("Volume name (eg: sdb, sdb1, sda5: )")} },
			{0}},

		show_user_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_QSTRING,  .name = {$ASCINI("User spec")}},
			{0}},

		show_vm_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_QSTRING,  .name = {$ASCINI("VM Id")}},
			{0}};


int	diff_action	( CLI_CTX *clictx, void *arg);
int	show_action	( CLI_CTX *clictx, void *arg);

CLI_VERB	show_what []  = {
	{ {$ASCINI("volume")},	.params = show_volume_params, .quals = show_volume_quals , .act_rtn = show_action, .act_arg = (void *)(intptr_t) SHOW$K_VOLUME },
	{ {$ASCINI("vm")},	.params = show_user_params, .quals = show_vm_quals,  .act_rtn = show_action, .act_arg = (void *)(intptr_t) SHOW$K_VM  },
	{ {$ASCINI("user")},	.params = show_vm_params, .quals = show_user_quals , .act_rtn = show_action, .act_arg = (void *)(intptr_t) SHOW$K_USER  },
	{0}};


CLI_VERB	top_commands []  = {
	{ .name = {$ASCINI("diff")}, .params = diff_params, .quals = diff_quals , .act_rtn = diff_action  },
	{ .name = {$ASCINI("show")}, .next = show_what},
	{ .name = {$ASCINI("sort")}, .params = sort_params, .quals = sort_quals , .act_rtn = sort_action  },
	{0}};

ASC	prompt = {$ASCINI("CRYPTORCP>")};


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__CLI_DEBUG__ demo harness - not part of the library API.]
**
**	Action routine bound to the demo DIFF verb: fetch its parameters and
**	qualifiers from the context and report them.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	arg	The verb's act_arg
**
**  RETURN VALUE:
**	A condition code.
**--
*/
int	diff_action	(
		CLI_CTX		*clictx,
			void	*arg
			)
{
int	status;
ASC	fl1, fl2;

	(void) arg;

	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "Action routine is just called!");

	status = __cli$get_value(clictx, &diff_params[0], &fl1);

	status = __cli$get_value(clictx, &diff_params[1], &fl2);

	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "Comparing %.*s vs %.*s", $ASC(&fl1), $ASC(&fl2));

	(void) status;

	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "Action routine has been completed!");

	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__CLI_DEBUG__ demo harness - not part of the library API.]
**
**	Action routine bound to the demo SORT verb.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	arg	The verb's act_arg
**
**  RETURN VALUE:
**	A condition code.
**--
*/
int	sort_action	(
		CLI_CTX		*clictx,
			void	*arg
			)
{
	(void) arg;

	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "sort action routine called");

	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__CLI_DEBUG__ demo harness - not part of the library API.]
**
**	Action routine bound to the demo SHOW verb.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	arg	The verb's act_arg
**
**  RETURN VALUE:
**	A condition code.
**--
*/
int	show_action	( CLI_CTX *clictx, void *arg)
{
int	what = (int)(intptr_t) arg;

	(void) clictx;

	switch	( what )
		{
		case	SHOW$K_VOLUME:
			break;

		case	SHOW$K_USER:
			break;

		case	SHOW$K_VM:
			break;

		default:
			$LOG(STS$K_FATAL, "Error processing SHOW command");
		}

	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__CLI_DEBUG__ demo harness - not part of the library API.]
**
**	Entry point of the demo: define a small grammar, parse argv with
**	__cli$parse() and dispatch the matched verb with __cli$dispatch().
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
int main	(int	argc, char **argv)
{
int	status;
void	*clictx = NULL;

	{

	struct tm _tm = {0};

	status = sscanf ("15-10-2018-15:17:13", "%2d-%2d-%4d%c%2d:%2d:%2d",
			&_tm.tm_mday, &_tm.tm_mon, &_tm.tm_year,
			(char *) &status,
			&_tm.tm_hour, &_tm.tm_min, &_tm.tm_sec);

	//return	0;
	}



	/* Dump to screen verbs definitions */
	__cli$show_verbs (top_commands, 0);

	/* Process command line arguments */
	if ( !(1 & (status = __cli$parse (top_commands, CLI$M_OPTRACE | CLI$M_OPSIGNAL, argc - 1, argv + 1, &clictx))) )
		return	-EINVAL;

	/* Show  a result of the parsing */
	__cli$show_ctx (clictx);

	/* Process command line arguments */
	if ( !(1 & (status = __cli$dispatch (clictx))) )
		return	-EINVAL;

	/* Release hass been allocated resources */
	__cli$cleanup(clictx);

	return 0;
}


#pragma GCC diagnostic pop

#ifdef __cplusplus
    }
#endif
#endif	/* __CLI_DEBUG__ */
