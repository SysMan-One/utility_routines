#ifndef	__CLI$ROUTINES__
#define __CLI$ROUTINES__	1

#ifdef __cplusplus
extern "C" {
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
**--
*/


/*
 * Parameters (P1 - P8)
 */
#define	CLI$K_P1	1	/* Position depended parameters	*/
#define	CLI$K_P2	2
#define	CLI$K_P3	3
#define	CLI$K_P4	4
#define	CLI$K_P5	5
#define	CLI$K_P6	6
#define	CLI$K_P7	7
#define	CLI$K_P8	8

#define	CLI$K_QUAL	9	/* Qualifier*/

				/* Parameter/Value - types	*/
#define	CLI$K_FILE	1
#define	CLI$K_DATE	2
#define	CLI$K_NUM	3	/* Number: Octal, Decimal, Hex	*/
#define	CLI$K_IPV4	4	/* 212.129.97.4			*/
#define	CLI$K_IPV6	5
#define	CLI$K_OPT	7	/* Options - no value		*/
#define	CLI$K_QSTRING	8	/* Quoted string		*/
#define	CLI$K_UUID	9	/* Fucken UUID			*/
#define	CLI$K_DEVICE	0xa	/* A device in the /dev/	*/

#define	CLI$K_KWD	0xb	/* A predefined keyword		*/


#define	CLI$M_NEGATABLE	1	/* Qualifier can be negatable	*/
#define	CLI$M_LIST	2
#define	CLI$M_PRESENT	4

#define	CLI$S_MAXVERBL	32	/* Maximum verb's length	*/

#ifndef	__ASC_TYPE__
typedef	struct __asc__	{
	unsigned char	len,
			sts[255];
} ASC;
#endif



typedef	struct __cli_keyword__	{
	ASC		name;	/* Keyword's string itself	*/
	unsigned long long val;	/* Associated value		*/
} CLI_KEYWORD;

typedef	struct __cli_pqdesc__	{
	ASC		name;	/* A short name of the parameter*/

	unsigned short	type;	/* FILE, DATE ...		*/
	unsigned char	pn;	/* P1, P2, ... P8		*/
	unsigned char	flag;	/* Reserved			*/

	ASC		defval;	/* Default value string		*/

	CLI_KEYWORD *	kwd;	/* A list of keywords		*/

} CLI_PQDESC;

typedef	struct __cli__verb__	{
	ASC		name;

	struct __cli__verb__ *next;

	CLI_PQDESC	*params;
	CLI_PQDESC	*quals;

	int	(*act_rtn) (__unknown_params);
	void	*act_arg;

} CLI_VERB;

typedef	struct	__cli_item__{
	struct	__cli_item__ *next;/* Linked list stuff	*/

	unsigned	type;	/* 0 - verb, P1 - P8, QUAL	*/

	union	{
		CLI_VERB	*verb;

				/* An address of the CLI's	*/
				/* parameters or qualifiers	*/
				/* definition structure		*/
		CLI_PQDESC	*pqdesc;
	};

	ASC		val;	/* Value string			*/

} CLI_ITEM;

/*
 *	A context to keep has been parsed command line:
 *		verb parameters, qualifiers
 */

/* Processing options		*/
#define	CLI$M_OPTRACE	1
#define	CLI$M_OPSIGNAL	2

typedef struct __cli_ctx__
{
	int	opts;

	CLI_ITEM	*vlist,	/* A list of verbs' sequence for a command */
			*avlist;/* A list of parameters' values and qualifiers */
} CLI_CTX;



/*
 * CLI API Routines declaration section
 */
void	cli$show_verbs	(CLI_VERB *verbs, int level);
int	cli$parse	(CLI_VERB *verbs, int opts, int	argc, char ** argv, void **clictx);
int	cli$dispatch	(CLI_CTX *clictx);
int	cli$cleanup	(CLI_CTX *clictx);
int	cli$get_value	(CLI_CTX *clictx, CLI_PQDESC *pq, ASC *val);

#ifdef __cplusplus
    }
#endif

#endif	/* __CLI$ROUTINES__ */
