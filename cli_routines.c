#define	__MODULE__	"CLI_ROUTINES"
#define	__IDENT__	"X.00-01"

#ifdef	__GNUC__
	#ident			__IDENT__

	#pragma GCC diagnostic ignored  "-Wparentheses"
	#pragma	GCC diagnostic ignored	"-Wdate-time"
	#pragma	GCC diagnostic ignored	"-Wunused-variable"
	#pragma GCC diagnostic ignored	"-Wmissing-braces"
#endif

#ifdef __cplusplus
    extern "C" {
#define __unknown_params ...
#define __optional_params ...
#else
#define __unknown_params
#define __optional_params ...
#endif

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
**  AUTHORS: Ruslan R. Laishev (RRL)
**
**  CREATION DATE:  22-AUG-2018
**
**  MODIFICATION HISTORY:
**
**	15-OCT-2018	RRL	A main part of code has been wrote; split CLI API to .C and .H modules.
**
**	28-JUL-2021	RRL	Project reanimation.
**
**--
*/

#include	<string.h>
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

#define $SHOW_PARM(name, value, format)	$IFTRACE(q_trace, ": " #name " = " format, (value))
#define $SHOW_PTR(var)			$SHOW_PARM(var, var, "%p")
#define $SHOW_STR(var)			$SHOW_PARM(var, (var ? var : "UNDEF(NULL)"), "'%s'")
#define $SHOW_INT(var)			$SHOW_PARM(var, ((int) var), "%d")
#define $SHOW_UINT(var)			$SHOW_PARM(var, ((unsigned) var), "%u")
#define $SHOW_ULL(var)			$SHOW_PARM(var, ((unsigned long long) var), "%llu")
#define $SHOW_UNSIGNED(var)		$SHOW_PARM(var, var, "0x%08x")
#define $SHOW_BOOL(var)			$SHOW_PARM(var, (var ? "ENABLED(TRUE)" : "DISABLED(FALSE)"), "%s");


static	char *cli$val_type	(
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
		}

	return	"ILLEGAL";
}

/*
 *
 *  DESCRIPTION: Check a input value for the parameter/qualifier corresponding has been declared type

 *
 *  INPUT:
 *	clictx:	CLI-context has been created by cli$parse()
 *	pqdesc:	Parameter/Qualifier descriptor
 *	val:	an ASCIC string to be checked
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	cli$val_check	(
		CLI_CTX		*clictx,
		CLI_PQDESC	*pqdesc,
			ASC	*val
				)
{
unsigned long long int	status = -1;
char	buf[NAME_MAX], *cp;

	switch (pqdesc->type)
		{
		case	CLI$K_IPV4:
		case	CLI$K_IPV6:
			if ( 1 !=  inet_pton(pqdesc->type == CLI$K_IPV4 ? AF_INET : AF_INET6, $ASCPTR(val), buf) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Value '%.*s' cannot be converted, errno=%d", $ASC(val), errno)
					: STS$K_ERROR;
			break;

		case	CLI$K_NUM:
			strtoull($ASCPTR(val), &cp, 0);

			if (errno == ERANGE )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Value '%.*s' cannot be converted, errno=%d", $ASC(val), errno)
					: STS$K_ERROR;

			break;

		case	CLI$K_DATE:
			{
			struct tm _tm = {0};

			if ( 3 > (status = sscanf ($ASCPTR(val), "%2d-%2d-%4d%c%2d:%2d:%2d",
					&_tm.tm_mday, &_tm.tm_mon, &_tm.tm_year,
					(char *) &status,
					&_tm.tm_hour, &_tm.tm_min, &_tm.tm_sec)) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Illformed date/time value '%.*s'",  $ASC(val))
					: STS$K_ERROR;
			break;
			}

		case	CLI$K_DEVICE:
			{
			struct stat st = {0};

			if ( !(cp = strstr($ASCPTR(val), "dev/")) )
				sprintf(cp  = buf, "/dev/%.*s", $ASC(val));
			else	cp = $ASCPTR(val);

			if ( stat(cp, &st) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "stat(%s), errno=%d", cp, errno)
					: STS$K_ERROR;
			break;
			}

		case	CLI$K_UUID:
			{
			if ( 6 != (status = sscanf ($ASCPTR(val), "%08x-%04x-%04x-%04x-%012x",
						   &status, &status, &status, &status, &status)) )
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_ERROR, "Illformat UUID value '%.*s'", $ASC(val))
					: STS$K_ERROR;

			break;
			}

		default:
			return	(clictx->opts & CLI$M_OPSIGNAL)
				? $LOG(STS$K_ERROR, "Unknown  type of value '%.*s'", $ASC(val))
				: STS$K_ERROR;


		}


	return	STS$K_SUCCESS;
}

static	int	cli$add_item2ctx	(
		CLI_CTX		*clictx,
		int		type,
		void		*item,
		char		*val
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

	if ( CLI$K_QUAL == type )
		avp->pqdesc = item;
	else	avp->pqdesc =  item;

	/* Run over list down to last item */
	for ( avp2 = clictx->avlist; avp2 && avp2->next; avp2 = avp2->next);

	/* Insert new item into the CLI context list */
	if ( avp2 )
		avp2->next = avp;
	else	clictx->avlist = avp;

	return	STS$K_SUCCESS;
}

void	cli$show_verbs	(
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
			cli$show_verbs(verb->next, ++level);

		for ( par = verb->params; par && par->pn; par++)
			{
			$LOG(STS$K_INFO, "   P%d - '%.*s' (%s)", par->pn, $ASC(&par->name), cli$val_type (par->type));

			if ( par->kwd )
				{
				for ( kwd = par->kwd; $ASCLEN(&kwd->name); kwd++)
					$LOG(STS$K_INFO, "      %.*s=%#x ", $ASC(&kwd->name), kwd->val);
				}
			}


		for ( qual = verb->quals; qual && $ASCLEN(&qual->name); qual++)
			{
			$LOG(STS$K_INFO, "   /%.*s (%s)", $ASC(&qual->name), cli$val_type (qual->type) );

			if ( qual->kwd )
				{
				for ( kwd = qual->kwd; $ASCLEN(&kwd->name); kwd++)
					$LOG(STS$K_INFO, "      %.*s=%#x ", $ASC(&kwd->name), kwd->val);
				}
			}

		}
}


void	cli$show_ctx	(
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
 *
 *  DESCRIPTION: check a given string against a table of keywords,
 *	detect ambiguous input:
 *		ST	- is conflicted with START & STOP
 *	allow not full length-comparing:
 *		SH	- is matched to  SHOW
 *
 *  INPUT:
 *	sts:	a keyword string to be checked
 *	opts:	processing options, see CLI$M_OP*
 *	klist:	an address of the keyword table, null entry terminated
 *
 *  OUTPUT:
 *	kwd:	an address to accept a pointer to keyword's record
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	cli$check_keyword	(
			char	*sts,
			int	 len,
		int		opts,
		CLI_KEYWORD	*klist,
		CLI_KEYWORD **	kwd
		)
{
CLI_KEYWORD	*krun, *ksel;
int	qlog = opts & CLI$M_OPTRACE;

	*kwd = NULL;

	for ( krun = klist; $ASCLEN(&krun->name); krun++)
		{
		if ( len > $ASCLEN(&krun->name) )
			continue;

		$IFTRACE(qlog, "Match [0:%d]='%.*s' against '%.*s'", len, len, sts, $ASC(&krun->name) );

		if ( !strncasecmp(sts, $ASCPTR(&krun->name), $MIN(len, $ASCLEN(&krun->name))) )
			{
			if ( ksel )
				{
				return	(opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_FATAL, "Ambiguous input '%.*s' (matched to : '%.*s', '%.*s')", len, sts, $ASC(&krun->name), $ASC(&ksel->name))
					: STS$K_FATAL;
				}

			/* Safe has been matched keyword's record */
			ksel = krun;
			}
		}

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
 *
 *  DESCRIPTION: extract a params list from the command line corresponding to definition
 *		of the param list from the 'verb'
 *
 *  INPUT:
 *	verbs:	verb's definition structure
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  IMPLICIT OUTPUT:
 *	ctx:	A CLI-context to be created
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	_cli$parse_quals(
	CLI_CTX	*	clictx,
	CLI_VERB	*verb,
		int	argc,
		char ** argv
			)
{
CLI_PQDESC	*qrun, *qsel = NULL;
int		status, len, i, qlog = clictx->opts & CLI$M_OPTRACE;
char		*aptr, *vptr;

	/*
	 *  Run over arguments from command line
	 */
	for ( qsel = NULL, aptr = argv[i = 0]; argc; argc--, i++,  aptr = argv[i] )
		{
		if ( (*aptr == '-') || (*aptr == '/') )
			aptr++;
		else	continue;

		/* Is there '=' and value ? */
		if ( vptr = strchr(aptr, '=') )
			len = vptr - aptr;
		else	len = strnlen(aptr, ASC$K_SZ);

		for ( qsel = NULL, qrun = verb->quals; $ASCLEN(&qrun->name); qrun++  )
			{
			if ( len > $ASCLEN(&qrun->name) )
				continue;

//			$IFTRACE(qlog, "Match [0:%d]='%.*s' against '%.*s'", len, len, aptr, $ASC(&qrun->name) );

			if ( !strncasecmp(aptr, $ASCPTR(&qrun->name), len) )
				{
				if ( qsel )
					{
					return	(clictx->opts & CLI$M_OPSIGNAL)
						? $LOG(STS$K_FATAL, "Ambiguous input '%.*s' (matched to : '%.*s', '%.*s')", len, aptr, $ASC(&qrun->name), $ASC(&qsel->name))
						: STS$K_FATAL;
					}

				vptr	+= (vptr != NULL);
				$IFTRACE(qlog, "%.*s='%s'", $ASC(&qrun->name), vptr);

				status = cli$add_item2ctx(clictx, CLI$K_QUAL, qrun, vptr);

				qsel = qrun;
				}
			}
		}

	return	STS$K_SUCCESS;
}

/*
 *
 *  DESCRIPTION: extract a params list from the command line corresponding to definition
 *		of the param list from the 'verb, add param/value pair into the 'CLI-context' area.
 *
 *  INPUT:
 *	clictx:	A CLI-context to be created
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	_cli$parse_params(
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
		$IFTRACE(qlog, "P%d(%.*s)='%s'", param->pn, $ASC(&param->name), argv[pi] );

		/* Put parameter's value into the CLI context */
		if ( !(1 & (status = cli$add_item2ctx (clictx, param->pn, param, argv[pi]))) )
			return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Error inserting P%d - %.*s into CLI context area", param->pn, $ASC(&param->name)) : STS$K_FATAL;
		}

	/* Did we get all parameters ? */
	if ( param && param->pn )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Missing P%d - %.*s !", param->pn, $ASC(&param->name)) : STS$K_FATAL;

	/* Now we can extract qualifiers ... */
	status = _cli$parse_quals(clictx, verb, argc - pi, argv + pi);

	return	status;
}

/*
 *
 *  DESCRIPTION: parsing input list of arguments by using a command's verbs definition is provided by 'verbs'
 *		syntax definition. Internaly performs command verb matching and calling other CLI-routines to parse 'parameters'
 *		and 'qualifiers'.
 *		Create a CLI-context area is supposed to be used by cli$get_value() routine to extract a parameter value.

 *  INPUT:
 *	verbs:	commands' verbs definition structure, null entry terminated
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  OUTPUT:
 *	ctx:	A CLI-context to be created
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	_cli$parse_verb	(
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
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Too many arguments") : STS$K_FATAL;


	pverb = argv[0];

	/*
	 * First at all we need to match  argv[1] in the verbs list
	 */
	len = strnlen(pverb, CLI$S_MAXVERBL);

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
	cli$add_item2ctx (clictx, 0, vsel, pverb);

	/* Is there a next subverb ? */
	if ( vsel->next )
		status = _cli$parse_verb	(clictx, vsel, argc - 1, argv++);
	else	{
		if ( ( !vsel->params) && (!vsel->quals) )
			return	STS$K_SUCCESS;

		status = _cli$parse_params(clictx, vsel, argc - 1, argv + 1);
		}

	return	status;
}


/*
 *
 *  DESCRIPTION: a top level routine - as main entry for the CLI parsing.
 *		Create a CLI-context area is supposed to be used by cli$get_value() routine to extract a parameter value.

 *  INPUT:
 *	verbs:	commands' verbs definition structure, null entry terminated
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  OUTPUT:
 *	ctx:	A CLI-context to be created
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
int	cli$parse	(
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
	 * Sanity check for input arguments ...
	 */
	if ( argc < 1 )
		return	(opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Too many arguments") : STS$K_FATAL;

	/* Create CLI-context area */
	if ( !(*clictx = calloc(1, sizeof(CLI_CTX))) )
		return	(opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Cannot allocate memory, errno=%d", errno) : STS$K_FATAL;
	ctx = *clictx;
	ctx->opts = opts;


	status = _cli$parse_verb(*clictx, verbs, argc, argv);

	return	STS$K_SUCCESS;
}

/*
 *
 *  DESCRIPTION: retreive a value of the parameter or qualifier from the CLI-context has been created and filled by cli$parse().

 *  INPUT:
 *	ctx:	A CLI-context has been created by cli$parse()
 *	pq:	A pointer to parameter/qualifier definition
 *	val:	A buffer to accept value
 *
 *  RETURN:
 *	STS$K_WARN	- value is zero length
 *	SS$_NORMAL, condition status
 *
 */
int	cli$get_value	(
	CLI_CTX		*clictx,
	CLI_PQDESC	*pq,
		ASC	*val
			)
{
CLI_ITEM *item;

	/* Sanity check */
	if ( !clictx )
		return	$LOG(STS$K_FATAL, "CLI-context is empty");

	if ( !pq )
		return	$LOG(STS$K_FATAL, "Illegal parameter/qualifier definition");

	for ( item = clictx->avlist; item; item = item->next)
		{
		if ( pq  == item->pqdesc )
			{
			/* Do we need to return a qualifier value to caller ?*/
			if ( val )
				{
				*val = item->val;

				if ( !($ASCLEN(val)) )
					return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_WARN, "Zero length value") : STS$K_WARN;
				}

			return	STS$K_SUCCESS;
			}

		}

	return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_ERROR, "No parameter/qualifier ('%.*s') is present in command line",
						       $ASC(&pq->name)) : STS$K_ERROR;
}


/*
 *
 *  DESCRIPTION: release resources has been allocated by cli$parse() routine.
 *
 *  INPUT:
 *	ctx:	A CLI-context has been created by cli$parse()
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
int	cli$cleanup	(
		CLI_CTX	*clictx
		)
{

CLI_ITEM	*avp, *avp2;

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
 *
 *  DESCRIPTION: dispatch processing to action routine has been defined for the last verb's keyword.
 *
 *  INPUT:
 *	ctx:	A CLI-context has been created by cli$parse()
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
int	cli$dispatch	(
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

	if ( verb->act_rtn )
		return	verb->act_rtn(clictx, verb->act_arg);

	return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_WARN, "No action routine has been defined") : STS$K_WARN;

}




#ifdef	__CLI_DEBUG__

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
	{ {$ASCINI("volume")},	.params = show_volume_params, .quals = show_volume_quals , .act_rtn = show_action, .act_arg = SHOW$K_VOLUME },
	{ {$ASCINI("vm")},	.params = show_user_params, .quals = show_vm_quals,  .act_rtn = show_action, .act_arg = SHOW$K_VM  },
	{ {$ASCINI("user")},	.params = show_vm_params, .quals = show_user_quals , .act_rtn = show_action, .act_arg = SHOW$K_USER  },
	{0}};


CLI_VERB	top_commands []  = {
	{ .name = {$ASCINI("diff")}, .params = diff_params, .quals = diff_quals , .act_rtn = diff_action  },
	{ .name = {$ASCINI("show")}, .next = show_what},
	{ .name = {$ASCINI("sort")}, .params = sort_params, .quals = sort_quals , .act_rtn = sort_action  },
	{0}};

ASC	prompt = {$ASCINI("CRYPTORCP>")};


int	diff_action	(
		CLI_CTX		*clictx,
			void	*arg
			)
{
int	status;
ASC	fl1, fl2;

	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "Action routine is just called!");

	status = cli$get_value(clictx, &diff_params, &fl1);

	status = cli$get_value(clictx, &diff_params, &fl2);

	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "Comparing %.*s vs %.*s", $ASC(&fl1), $ASC(&fl2));


	$IFTRACE(clictx->opts & CLI$M_OPTRACE, "Action routine has been completed!");

	return	STS$K_SUCCESS;
}

int	show_action	( CLI_CTX *clictx, void *arg)
{
int	what = (int) arg, status;
ASC	val;


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
	cli$show_verbs (top_commands, 0);

	/* Process command line arguments */
	if ( !(1 & (status = cli$parse (top_commands, CLI$M_OPTRACE | CLI$M_OPSIGNAL, argc - 1, argv + 1, &clictx))) )
		return	-EINVAL;

	/* Show  a result of the parsing */
	cli$show_ctx (clictx);

	/* Process command line arguments */
	if ( !(1 & (status = cli$dispatch (clictx))) )
		return	-EINVAL;

	/* Release hass been allocated resources */
	cli$cleanup(clictx);

	return 0;
}


#ifdef __cplusplus
    }
#endif
#endif	/* __CLI_DEBUG__ */
