#define	__MODULE__	"CLI_ROUTINES"
#define	__IDENT__	"X.00-17ECO1"
#define	__REV__		"0.17.1"

/*
**++
**
**  FACILITY:  Command Language Interface (CLI) Routines
**
**  ABSTRACT: A portable command line parser modelled on the OpenVMS CDU/CLI: it matches argv against a table of verbs
**	(with sub-verbs), parameters and qualifiers by the DCL rules (abbreviations, /NO<name>, value lists, keywords),
**	validates the value types, builds a parse context and dispatches the action routine of the verb.  Syntax:
**	<verb> [p1 p2 ... p8] [/qualifiers[=<value> ...]]; the Unix form --name is accepted for a qualifier.  /HELP, -h and --help
**	print a usage screen made from the tables.  Needs starlet (__util$bintim() for CLI$K_TIME).
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
**	27-JUL-2026	RRL	X.00-02 : Audit fixes: __cli$parse() propagates the parse status and frees the half-built context; __cli$val_check()
**				rewritten (UUID, device path, number checks) and wired up, __cli$check_keyword() wired up; sub-verb recursion and NULL guards fixed.
**
**	28-JUL-2026	RRL	X.00-03 : Routine prefix cli$/_cli$ renamed to __cli$ (the CLI$K_* constants keep CLI$).
**
**	30-JUL-2026	RRL	X.00-04 : Documentation pass - VMS-style **++ headers on every routine; __CLI_DEBUG__ demo documented; normalised to LF.
**
**	31-JUL-2026	RRL	X.00-05 : Qualifier defaults (defval), negatable qualifiers (CLI$M_NEGATABLE), value lists (CLI$M_LIST), required
**				parameters/qualifiers (CLI$M_REQUIRED) and __cli$present().
**
**	01-AUG-2026	RRL	X.00-06 : Value types CLI$K_TIME (via __util$bintim) and CLI$K_MAC; the module depends on starlet now.
**
**	01-AUG-2026	RRL	X.00-07 : cli_routines.h self-contained: CLI_VERB.act_rtn has a full prototype (CLI_CTX *, void *), no __unknown_params (GCC 14, C23).
**
**	01-AUG-2026	RRL	X.00-08 : Qualifier lookup and value fixes: NULL verb->quals, zero length token, exact name wins over a longer one it prefixes
**				(/LOG vs /LOGFILE, INET vs INET6), drained CLI$M_LIST loop, optional parameters; <strings.h> added, NULL + 0 removed.
**
**	01-AUG-2026	RRL	X.00-09 : __cli$help() (usage generator from the tables) and the built-in /HELP, -h, --help; help is detected before
**				parameter validation and the parse returns STS$K_WARN.
**
**	01-AUG-2026	RRL	X.00-11 : Help detector narrowed: the leading '-' or '/' is required (a verb or a value "help" is not swallowed); __cli$parse()
**				zeroes *clictx up front. See tests/test_cli_help.c.
**
**	01-AUG-2026	RRL	X.00-12 : __cli$present() returns the state of the LAST occurrence of a repeated qualifier (DCL "last wins"). See tests/test_cli_features.c.
**
**	 3-AUG-2026	RRL	X.00-13 : The Unix qualifier prefix is the GNU form '--name' instead of '-name'; the short -h help alias stays.
**
**	 3-AUG-2026	RRL	X.00-14 : __cli$parse_verb() descends into the sub-verb list (vsel->next), so SHOW STATISTICS is found. See tests/test_cli_nested.c.
**
**	 7-AUG-2026	RRL	X.00-15 : __cli$parse_params() stops the positional scan at a defined qualifier; a '/'-path stays a parameter value. See tests/test_cli_filepath.c.
**
**	 7-AUG-2026	RRL	X.00-16 : A bare token which is neither a parameter nor a qualifier is reported ('Unrecognized token') instead of dropped.
**
**	14-AUG-2026	RRL	X.00-16ECO1 : __REV__ read "0.02.0" against __IDENT__ X.00-16; per the Xaa-bbECOc -> aa.bb.c rule it is "0.16.1".
**
**	22-OCT-2026	RRL	X.00-16ECO2 : MinGW-w64 build from Linux: inet_pton() from <winsock2.h>/<ws2tcpip.h>, NAME_MAX defined as 255.
**
**	31-OCT-2026	CLD	X.00-17ECO1 : StarLet 1.7.1: __cli$get_keyword() returns STS$K_WARN for an item present with an empty value (/mode, /mode=), as __cli$get_value() does.
**
**	31-OCT-2026	CLD	X.00-17 : StarLet 1.7.0 DCL parsing rules: verb abbreviation (full name wins, unique prefix selects, synonyms never ambiguous),
**				"Too many parameters", /HELP of a sub-verb, CLI$K_KWD stored as the full keyword and new __cli$get_keyword(), unknown /NAME or --name
**				always an error, quiet STS$K_ERROR "absent" in __cli$get_value(), stricter CLI$K_DATE/CLI$K_NUM, __cli$show_verbs() lists a sub-verb table once.
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
#ifdef	_WIN32
#include	<winsock2.h>		/* inet_pton() */
#include	<ws2tcpip.h>
#else
#include	<arpa/inet.h>
#endif
#ifndef	NAME_MAX
#define	NAME_MAX	255		/* Windows has no NAME_MAX; 255 is the NTFS file name limit as well */
#endif

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

/* S_CLI$DIGITS - read 1..a_maxdigits decimal digits at *a_pp and advance it; STS$K_ERROR if there is none */
static	int	s_cli$digits	(
	const char	**a_pp,
		int	a_maxdigits,
		int	*a_val
			)
{
const char	*l_p = *a_pp;
int		l_n = 0, l_v = 0;

	for ( ; (l_n < a_maxdigits) && (*l_p >= '0') && (*l_p <= '9'); l_p++, l_n++ )
		l_v = l_v * 10 + (*l_p - '0');

	if ( !l_n )
		return	STS$K_ERROR;

	*a_pp = l_p;
	*a_val = l_v;

	return	STS$K_SUCCESS;
}

/* S_CLI$DATE_CHECK - validate dd-mm-yyyy[<sep>hh:mm[:ss]], <sep> is '-', a blank or 'T'; calendar and leap years checked, no trailing text */
static	int	s_cli$date_check	(
	const char	*a_val
				)
{
static const int s_dim[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
const char	*l_p = a_val;
int		l_day, l_mon, l_year, l_hour = 0, l_min = 0, l_sec = 0, l_dmax;

	if ( !(1 & s_cli$digits(&l_p, 2, &l_day)) || (*l_p++ != '-') || !(1 & s_cli$digits(&l_p, 2, &l_mon)) || (*l_p++ != '-') || !(1 & s_cli$digits(&l_p, 4, &l_year)) )
		return	STS$K_ERROR;

	if ( (l_mon < 1) || (l_mon > 12) || (l_year < 1) )
		return	STS$K_ERROR;

	l_dmax = s_dim[l_mon - 1] + ((l_mon == 2) && !(l_year % 4) && ((l_year % 100) || !(l_year % 400)));

	if ( (l_day < 1) || (l_day > l_dmax) )
		return	STS$K_ERROR;

	if ( *l_p )
		{
		if ( (*l_p != '-') && (*l_p != ' ') && (*l_p != 'T') )
			return	STS$K_ERROR;
		l_p++;

		if ( !(1 & s_cli$digits(&l_p, 2, &l_hour)) || (*l_p++ != ':') || !(1 & s_cli$digits(&l_p, 2, &l_min)) )
			return	STS$K_ERROR;

		if ( *l_p == ':' )
			{
			l_p++;
			if ( !(1 & s_cli$digits(&l_p, 2, &l_sec)) )
				return	STS$K_ERROR;
			}

		if ( *l_p || (l_hour > 23) || (l_min > 59) || (l_sec > 59) )
			return	STS$K_ERROR;
		}

	return	STS$K_SUCCESS;
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
**	Validate a candidate value string against the value type of a descriptor (CLI$K_FILE, CLI$K_NUM,
**	CLI$K_IPV4, ...).  A helper of __cli$parse().
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	pqdesc	The parameter/qualifier descriptor
**	a_valp	In: the candidate value.  For a CLI$K_KWD replaced with the FULL keyword of the table
**		(copied into <a_canon>), left alone for other types
**	a_canon	A buffer of ASC$K_SZ + 1 octets for the full keyword
**
**  RETURN VALUE:
**	STS$K_SUCCESS if the value is valid for its type; error otherwise.
**--
*/
static	int	__cli$val_check	(
		CLI_CTX		*clictx,
		CLI_PQDESC	*pqdesc,
	const	char		**a_valp,
		char		*a_canon
				)
{
char		buf[NAME_MAX], *cp;
const char	*a_val = *a_valp;

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

			/* The values are unsigned: strtoull() would wrap "-1" to ULLONG_MAX and skip a leading blank;
			** neither is a number here. A range overflow (ERANGE) and trailing junk are errors too.
			*/
			if ( (*a_val == '-') || (*a_val == ' ') || (*a_val == '\t') )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Value '%s' is not a valid unsigned number", a_val)
					: STS$K_ERROR;

			errno = 0;
			strtoull(a_val, &l_end, 0);

			if ( (errno == ERANGE) || (l_end == a_val) || (l_end && *l_end) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Value '%s' is not a valid number", a_val)
					: STS$K_ERROR;
			}
			break;

		case	CLI$K_DATE:
			if ( !(1 & s_cli$date_check(a_val)) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Illformed date/time value '%s'", a_val)
					: STS$K_ERROR;
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
			int		l_status;

			if ( !pqdesc->kwd )
				break;			/* no keyword list - nothing to check against */

			if ( !(1 & (l_status = __cli$check_keyword((char *) a_val, (int) strlen(a_val), clictx->opts, pqdesc->kwd, &l_kwd))) )
				return	l_status;

			/* Keep the keyword of the table, not what has been typed (an abbreviation, other case) */
			memcpy(a_canon, $ASCPTR(&l_kwd->name), $ASCLEN(&l_kwd->name));
			a_canon[$ASCLEN(&l_kwd->name)] = '\0';
			*a_valp = a_canon;
			}
			break;

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

/* __CLI$ADD_ITEM2CTX - link a matched parameter/qualifier and its value into the context (CLI$M_PRESENT is added to <state>) */
static	int	__cli$add_item2ctx	(
		CLI_CTX		*clictx,
		int		type,
		void		*item,
	const	char		*val,
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
	avp->type = type;

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
**	Write a listing of a command-definition table to the log.  A sub-verb table is listed once, one level
**	deeper than its parent; the caller's level is not changed.
**
**  FORMAL PARAMETERS:
**
**	verbs	The command-definition table
**	level	Initial indentation level; 0 for the top level
**--
*/
void	__cli$show_verbs	(
	CLI_VERB	*verbs,
		int	level
		)
{
CLI_VERB *verb;
CLI_PQDESC *par;
CLI_KEYWORD *kwd;
CLI_PQDESC *qual;
char	spaces [64];

	memset(spaces, ' ', sizeof(spaces));

	for ( verb = verbs; $ASCLEN(&verb->name); verb++)
		{
		if ( !level )
			$LOG(STS$K_INFO, "Show verb '%.*s' : ", $ASC(&verb->name));
		else	$LOG(STS$K_INFO, " %.*s%.*s", $MIN(level * 2, (int) sizeof(spaces)), spaces, $ASC(&verb->name));

		/* A sub-verb table is listed once, one level deeper; our own level stays as it is */
		if ( verb->next )
			__cli$show_verbs(verb->next, level + 1);

		for ( par = verb->params; par && par->pn; par++)
			{
			$LOG(STS$K_INFO, "   P%d - '%.*s' (%s)", par->pn, $ASC(&par->name), __cli$val_type (par->type));

			if ( par->kwd )
				{
				for ( kwd = par->kwd; $ASCLEN(&kwd->name); kwd++)
					$LOG(STS$K_INFO, "      %.*s=%#llx ", $ASC(&kwd->name), (unsigned long long) kwd->val);
				}
			}


		for ( qual = verb->quals; qual && $ASCLEN(&qual->name); qual++)
			{
			$LOG(STS$K_INFO, "   /%.*s (%s)", $ASC(&qual->name), __cli$val_type (qual->type) );

			if ( qual->kwd )
				{
				for ( kwd = qual->kwd; $ASCLEN(&kwd->name); kwd++)
					$LOG(STS$K_INFO, "      %.*s=%#llx ", $ASC(&kwd->name), (unsigned long long) kwd->val);
				}
			}

		}
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write the matched verb, parameters and qualifiers with their values to the log.  For debugging.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context to display
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
		$LOG(STS$K_INFO, "%.*s %.*s  ('%.*s')", $MIN(splen, (int) sizeof(spaces)), spaces, $ASC(&avp->verb->name), $ASC(&avp->val));

	for ( avp = clictx->avlist; avp; avp = avp->next)
		{
		if ( avp->type != CLI$K_QUAL )
			$LOG(STS$K_INFO, "   P%d[0:%d]='%.*s'", avp->pqdesc->pn, $ASCLEN(&avp->val), $ASC(&avp->val));
		else	$LOG(STS$K_INFO, "   /%.*s[0:%d]='%.*s'", $ASC(&avp->pqdesc->name), $ASCLEN(&avp->val), $ASC(&avp->val));
		}
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Print a usage screen from the command tables: the list of commands (<verb> == NULL), the sub-verbs of a
**	container verb, or a SYNOPSIS plus the parameters and qualifiers of a leaf verb.  Reached from
**	__cli$parse() on /HELP, -h or --help.
**
**  FORMAL PARAMETERS:
**
**	verbs	The top-level command table (used when <verb> is NULL)
**	verb	The verb to describe, or NULL for an overview
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the log destination, unprefixed, via __util$out().
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

/* __CLI$CHECK_KEYWORD - match a token against a keyword list (exact name wins, else a unique abbreviation); error if unknown or ambiguous */
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

/* __CLI$PARSE_QUALS - parse the /qualifier[=value] words against the qualifier table of the verb, validating the values; helper of __cli$parse() */
/* S_CLI$IS_HELP - non-zero for /HELP, -HELP, --help, -h; the leading '-' or '/' is required, a bare "help" is an ordinary token (a verb or a value) */
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


/* S_CLI$FIND_QUAL - find a qualifier by an unambiguous prefix of its name (reqflag restricts the search to flagged ones, *ambig reports ambiguity); NULL if none */
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
char		*aptr, *vptr, l_canon[ASC$K_SZ + 1];
const char	*l_cval;

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
		else	{
			/*
			 * A bare token here took neither a parameter nor a qualifier -
			 * the parameters are already filled (or a qualifier stopped the
			 * scan) and this is not a qualifier. Report it rather than drop it
			 * silently: it is a surplus parameter, or a value which stands after
			 * a space instead of '=' ("/blocks 5" - the DCL wants "/blocks=5" as well).
			 */
			char	*l_prev = i ? argv[i - 1] : NULL;

			if ( l_prev && ((l_prev[0] == '/') || ((l_prev[0] == '-') && (l_prev[1] == '-'))) && !strchr(l_prev, '=') )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_FATAL, "Value '%s' must follow '=' of the qualifier '%s' (write %s=%s)", aptr, l_prev, l_prev, aptr)
					: STS$K_FATAL;

			return	(clictx->opts & CLI$M_OPSIGNAL)
				? $LOG(STS$K_FATAL, "Too many parameters, surplus token '%s'", aptr)
				: STS$K_FATAL;
			}

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
			char	*p = vptr, l_elem[ASC$K_SZ + 1], l_canon[ASC$K_SZ + 1];
			const char *l_pval;
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
					l_pval = l_elem;

					if ( !(1 & (status = __cli$val_check(clictx, qsel, &l_pval, l_canon))) )
						return	status;

					if ( !(1 & (status = __cli$add_item2ctx (clictx, CLI$K_QUAL, qsel, l_pval, 0))) )
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
		l_cval = vptr;

		if ( vptr && *vptr )
			if ( !(1 & (status = __cli$val_check(clictx, qsel, &l_cval, l_canon))) )
				return	status;

		if ( !(1 & (status = __cli$add_item2ctx (clictx, CLI$K_QUAL, qsel, l_cval, 0))) )
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

/* __CLI$PARSE_PARAMS - bind the positional parameters P1..P8 to the parameter table of the verb, validating the values; helper of __cli$parse() */
static	int	__cli$parse_params(
	CLI_CTX	*	clictx,
	CLI_VERB	*verb,
		int	argc,
		char ** argv
			)
{
CLI_PQDESC	*param;
int		status, pi, qlog = clictx->opts & CLI$M_OPTRACE;
char		l_canon[ASC$K_SZ + 1];
const char	*l_cval;

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

				/* "--name" which is none of the verb's qualifiers is always a mistyped qualifier. So is a
				** "/NAME", unless the pending parameter is a FILE or a DEVICE: then it is a path (/tmp,
				** /home, /etc/passwd) and stays that parameter's value. A token which matches a
				** qualifier, as an abbreviation too, was taken as the qualifier above.
				*/
				if ( (l_qlen > 0) && ((argv[pi][0] == '-') || ((param->type != CLI$K_FILE) && (param->type != CLI$K_DEVICE))) )
					return	(clictx->opts & CLI$M_OPSIGNAL)
						? $LOG(STS$K_FATAL, "Unrecognized qualifier '%.*s'", l_qlen, l_qn)
						: STS$K_FATAL;
				}
			}

		$IFTRACE(qlog, "P%d(%.*s)='%s'", param->pn, $ASC(&param->name), argv[pi] );

		/* Validate the value against the parameter's declared type */
		l_cval = argv[pi];

		if ( argv[pi] && *argv[pi] )
			if ( !(1 & (status = __cli$val_check(clictx, param, &l_cval, l_canon))) )
				return	status;

		/* Put parameter's value into the CLI context */
		if ( !(1 & (status = __cli$add_item2ctx (clictx, param->pn, param, l_cval, 0))) )
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

/* S_CLI$VERB_SYN - 1 if two verb entries are synonyms (same action, parameters, qualifiers and sub-verbs; entries with none of these are not) */
static	int	s_cli$verb_syn	(
	CLI_VERB	*a_v1,
	CLI_VERB	*a_v2
			)
{
	if ( !(a_v1->act_rtn || a_v1->params || a_v1->quals || a_v1->next) )
		return	0;

	return	(a_v1->act_rtn == a_v2->act_rtn) && (a_v1->act_arg == a_v2->act_arg) && (a_v1->params == a_v2->params)
		&& (a_v1->quals == a_v2->quals) && (a_v1->next == a_v2->next);
}

/* __CLI$PARSE_VERB - match the leading token against a verb table (DCL abbreviation rules) and descend into the sub-verb chain; helper of __cli$parse() */
static	int	__cli$parse_verb	(
	CLI_CTX		*clictx,
	CLI_VERB *	verbs,
		int	argc,
		char ** argv
			)
{
CLI_VERB	*vrun, *vsel, *vexact, *vamb = NULL;
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

	/* An empty word would be a prefix of every verb */
	if ( len < 1 )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Empty command verb") : STS$K_FATAL;

	/*
	 * Match the verb from the command line against the verbs table by the DCL rules
	 * (the same as __util$lookup_key() in the ABBREV mode): a verb spelled out in full
	 * wins - so SET is SET even if SETUP is defined; otherwise the word must be a prefix
	 * of ONE verb ("setu" -> SETUP), verbs which are synonyms (see s_cli$verb_syn())
	 * count as one; two different verbs make it ambiguous.
	 */
	for (vrun = verbs, vsel = NULL, vexact = NULL; $ASCLEN(&vrun->name); vrun++)
		{
		if ( len > $ASCLEN(&vrun->name) )
			continue;

		$IFTRACE(qlog, "Matching '%.*s' vs '%.*s' ... ", len, pverb, $ASC(&vrun->name));

		if ( strncasecmp(pverb, $ASCPTR(&vrun->name), len) )
			continue;

		if ( len == $ASCLEN(&vrun->name) )
			{
			if ( !vexact )
				vexact = vrun;
			continue;
			}

		/* An abbreviation: a second DIFFERENT verb makes it ambiguous */
		if ( vsel && !s_cli$verb_syn(vsel, vrun) )
			vamb = vrun;
		else if ( !vsel )
			vsel = vrun;
		}

	if ( vexact )
		vsel = vexact;
	else if ( vamb )
		return	(clictx->opts & CLI$M_OPSIGNAL)
			? $LOG(STS$K_FATAL, "Ambiguous input '%.*s' (matched to : '%.*s', '%.*s')", len, pverb, $ASC(&vamb->name), $ASC(&vsel->name))
			: STS$K_FATAL;

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

	/* A container verb (it has sub-verbs): help directly after it, "SHOW /HELP", lists the sub-verbs;
	** anything else is the sub-verb's word, looked up in vsel->next - the sub-verb owns the rest of
	** the line, including its own /HELP ("SHOW STATISTICS /HELP"). A sub-verb follows its parent
	** immediately: no qualifier stands between them.
	*/
	if ( vsel->next )
		{
		if ( (argc > 1) && s_cli$is_help(argv[1]) )
			{
			__cli$help (NULL, vsel);
			return	STS$K_WARN;
			}

		return	__cli$parse_verb	(clictx, vsel->next, argc - 1, argv + 1);
		}

	/* Built-in help: if any following token requests it, show this verb's
	** usage and stop before parameter/qualifier validation.
	*/
	for ( int l_i = 1; l_i < argc; l_i++ )
		if ( s_cli$is_help(argv[l_i]) )
			{
			__cli$help (NULL, vsel);
			return	STS$K_WARN;
			}

	/* A verb which takes neither parameters nor qualifiers takes no more words: DCL's "too many parameters" */
	if ( !vsel->params && !vsel->quals )
		{
		if ( argc > 1 )
			return	(clictx->opts & CLI$M_OPSIGNAL)
				? $LOG(STS$K_FATAL, "Too many parameters for '%.*s', surplus token '%s'", $ASC(&vsel->name), argv[1])
				: STS$K_FATAL;

		return	STS$K_SUCCESS;
		}

	status = __cli$parse_params(clictx, vsel, argc - 1, argv + 1);

	return	status;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Match the program arguments against a command-definition table, validate the verb, parameters and
**	qualifiers, and build a context for __cli$dispatch() and __cli$get_value(), to be released with
**	__cli$cleanup().  A verb spelled out in full wins over a longer one it prefixes (SET vs SETUP), a
**	unique abbreviation selects, an ambiguous one is an error; verbs sharing action, parameters, qualifiers
**	and sub-verbs are synonyms.  A "/name" with no match is the value of a pending CLI$K_FILE or
**	CLI$K_DEVICE parameter (a path), otherwise an "Unrecognized qualifier" error, as is any "--name".
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
**	Allocates the context; *clictx is NULL after an error or help.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on a clean parse; STS$K_FATAL (low bit clear) on a bad value/verb/qualifier, a surplus
**	word or an allocation failure.
**	STS$K_WARN (low bit clear too) is NOT an error: the command line asked for help, it has been printed
**	by __cli$help() and the program should exit successfully WITHOUT dispatching.
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
**	Retrieve the value bound to a parameter or qualifier.  For a CLI$M_LIST qualifier successive calls
**	return successive values, then report absence.  An absent item with a non-empty <defval> returns
**	the default.
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	pq	The parameter/qualifier descriptor
**	val	Receives the value as an ASCIC string
**
**  RETURN VALUE:
**	STS$K_SUCCESS if a value was returned; STS$K_WARN if the item is present with an empty value;
**	STS$K_ERROR if the item is absent (no default) or the list is drained - not logged, with
**	CLI$M_OPSIGNAL either.  A NULL context or descriptor is a programming error and is logged.
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

	/* Absent, or a list drained: a normal answer to a probe, so it is never logged, OPSIGNAL or not */
	return	STS$K_ERROR;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Resolve the value of a CLI$K_KWD item to its entry of the keyword table (CLI_PQDESC.kwd): the full
**	keyword name and its <val>.  The parse stores the FULL keyword, not what was typed.  Does not advance
**	the CLI$M_LIST cursor of __cli$get_value().
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	pq	The descriptor, a CLI$K_KWD one with a keyword list
**	val	A value from __cli$get_value(), or NULL for the first value of the item (or its default)
**	kwd	Receives the keyword entry (inside the table of <pq>)
**
**  RETURN VALUE:
**	STS$K_SUCCESS; STS$K_WARN if the item is present with an empty value (/mode, /mode=), as __cli$get_value()
**	says it; STS$K_ERROR if there is no keyword list, the item is absent without a default, or
**	the value is not a keyword of the table (nothing is logged).
**--
*/
int	__cli$get_keyword	(
	CLI_CTX		*clictx,
	CLI_PQDESC	*pq,
	const ASC	*val,
	CLI_KEYWORD	**kwd
			)
{
CLI_ITEM	*item;
const ASC	*l_val = val;
CLI_KEYWORD	*krun;

	if ( !clictx || !pq || !kwd )
		return	$LOG(STS$K_FATAL, "Illegal parameter(s) of __cli$get_keyword()");

	*kwd = NULL;

	if ( !pq->kwd )
		return	STS$K_ERROR;

	if ( !l_val )
		{
		for ( item = clictx->avlist; item && (item->pqdesc != pq); item = item->next);

		if ( item )
			l_val = &item->val;
		else if ( $ASCLEN(&pq->defval) )
			l_val = &pq->defval;
		else	return	STS$K_ERROR;
		}

	if ( !$ASCLEN(l_val) )
		return	STS$K_WARN;

	for ( krun = pq->kwd; $ASCLEN(&krun->name); krun++ )
		if ( ($ASCLEN(&krun->name) == $ASCLEN(l_val)) && !strncasecmp($ASCPTR(&krun->name), $ASCPTR(l_val), $ASCLEN(l_val)) )
			{
			*kwd = krun;
			return	STS$K_SUCCESS;
			}

	return	STS$K_ERROR;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Report whether a parameter or qualifier was supplied, and in what form (the CLI$PRESENT analogue).
**
**  FORMAL PARAMETERS:
**
**	clictx	The parse context
**	pq	The parameter/qualifier descriptor to test
**	state	Optional; receives CLI$M_PRESENT, plus CLI$M_NEGATED for /NO<name>; zero when absent.
**		A repeated qualifier reports its LAST occurrence (/HIGHLIGHT /NOHIGHLIGHT)
**
**  RETURN VALUE:
**	STS$K_SUCCESS if present in any form; STS$K_WARN if absent.
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
**	Free a parse context and its items; the pointer must not be used afterwards.
**
**  FORMAL PARAMETERS:
**
**	clictx	The context to release
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
**	Invoke the action routine of the matched verb with the context and the verb's act_arg.
**
**  FORMAL PARAMETERS:
**
**	clictx	A context produced by __cli$parse()
**
**  RETURN VALUE:
**	The condition value the action routine returns.
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


/* DIFF_ACTION - action routine of the demo DIFF verb (__CLI_DEBUG__ harness, not library API) */
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

/* SORT_ACTION - action routine of the demo SORT verb (__CLI_DEBUG__ harness, not library API) */
int	sort_action	(
		CLI_CTX		*clictx,
			void	*arg
			)
{
	(void) arg;

	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "sort action routine called");

	return	STS$K_SUCCESS;
}

/* SHOW_ACTION - action routine of the demo SHOW verb (__CLI_DEBUG__ harness, not library API) */
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

/* MAIN - __CLI_DEBUG__ demo: define a small grammar, __cli$parse() argv, __cli$dispatch() the verb; not library API */
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
