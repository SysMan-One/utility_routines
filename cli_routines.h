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
**  ABSTRACT: A portable API to parse command line parameters, check the syntax and dispatch
**	processing, modelled on the OpenVMS CDU/CLI facilities:
**	<verb> [p1 p2 ... p8] [/qualifiers[=<value> ...]]
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  22-AUG-2018
**
**  MODIFICATION HISTORY:
**
**	31-OCT-2026	CLD	X.00-17ECO1 : __cli$get_keyword() returns STS$K_WARN for an item present with an empty value.
**
**	31-OCT-2026	CLD	X.00-17 : CLI_PQDESC.flag comment corrected (the field is live);
**				__cli$show_ctx() and the new __cli$get_keyword() declared.
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
#define	CLI$K_UUID	9	/* UUID				*/
#define	CLI$K_DEVICE	0xa	/* A device in the /dev/	*/

#define	CLI$K_KWD	0xb	/* A predefined keyword		*/
#define	CLI$K_TIME	0xc	/* Absolute/delta time (__util$bintim)	*/
#define	CLI$K_MAC	0xd	/* MAC address aa:bb:cc:dd:ee:ff	*/


/* Descriptor flags (CLI_PQDESC.flag) - describe how a qualifier behaves.	*/
#define	CLI$M_NEGATABLE	1	/* Qualifier accepts a /NO<name> form		*/
#define	CLI$M_LIST	2	/* Accepts a value list: /Q=(a,b,c) or repeats	*/
#define	CLI$M_REQUIRED	0x10	/* Must be supplied, else the parse fails	*/

/* Item state flags (CLI_ITEM.state) - reported by __cli$present().	*/
#define	CLI$M_PRESENT	4	/* The item was given on the command line	*/
#define	CLI$M_NEGATED	8	/* Given in the negated form (/NO<name>)		*/
#define	CLI$M_CONSUMED	0x20	/* Already returned by __cli$get_value()		*/

#define	CLI$SSZ_MAXVERB	32	/* Maximum verb's length	*/

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
	unsigned char	flag;	/* CLI$M_NEGATABLE, CLI$M_LIST, CLI$M_REQUIRED	*/

	ASC		defval;	/* Default value string		*/

	CLI_KEYWORD *	kwd;	/* A list of keywords		*/

} CLI_PQDESC;

struct __cli_ctx__;		/* forward reference for the action prototype	*/

typedef	struct __cli__verb__	{
	ASC		name;

	struct __cli__verb__ *next;

	CLI_PQDESC	*params;
	CLI_PQDESC	*quals;

	int	(*act_rtn) (struct __cli_ctx__ *clictx, void *act_arg);
	void	*act_arg;

} CLI_VERB;

typedef	struct	__cli_item__{
	struct	__cli_item__ *next;/* Linked list stuff	*/

	unsigned	type;	/* 0 - verb, P1 - P8, QUAL	*/
	unsigned char	state;	/* Runtime: CLI$M_PRESENT/_NEGATED/_CONSUMED */

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
void	__cli$show_verbs	(CLI_VERB *verbs, int level);
int	__cli$parse	(CLI_VERB *verbs, int opts, int	argc, char ** argv, void **clictx);
int	__cli$dispatch	(CLI_CTX *clictx);
int	__cli$cleanup	(CLI_CTX *clictx);
int	__cli$get_value	(CLI_CTX *clictx, CLI_PQDESC *pq, ASC *val);
int	__cli$present	(CLI_CTX *clictx, CLI_PQDESC *pq, unsigned *state);
int	__cli$help	(CLI_VERB *verbs, CLI_VERB *verb);
void	__cli$show_ctx	(CLI_CTX *clictx);

/*
 * __cli$get_keyword - resolve a CLI$K_KWD value to its entry of the keyword table.
 *	The parse stores the FULL keyword of the table (not an abbreviation as typed), so
 *	a value from __cli$get_value() - or NULL for the item's first value/default - is
 *	looked up exactly. *kwd receives the CLI_KEYWORD (name and .val); nothing is logged.
 *	Returns STS$K_SUCCESS; STS$K_WARN when the item is present with an empty value (as __cli$get_value()); STS$K_ERROR when absent or not a keyword of the table.
 *
 * __cli$parse - returns STS$K_SUCCESS; STS$K_WARN (low bit clear, NOT an error) when help
 *	has been printed and nothing must be dispatched; STS$K_FATAL on a bad command line.
 *
 * __cli$get_value - STS$K_SUCCESS; STS$K_WARN present with an empty value; STS$K_ERROR
 *	absent or list drained (never logged, whatever CLI$M_OPSIGNAL says).
 */
int	__cli$get_keyword	(CLI_CTX *clictx, CLI_PQDESC *pq, const ASC *val, CLI_KEYWORD **kwd);

#ifdef __cplusplus
    }
#endif

#endif	/* __CLI$ROUTINES__ */
