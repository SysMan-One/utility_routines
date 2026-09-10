#define	__MODULE__	"AVPROTO"
#define	__IDENT__	"X.94-11ECO1"
#define	__REV__		"94.11.1"


/*
**++
**
**  FACILITY:  AVProto - Attribute/Value Pair (Tag Length Value) based protocol
**
**  ABSTRACT: Carrying data encapsulated in to the AVP microcontainers as payload of the protocol data unit (PDU)
**
**  DESCRIPTION: This module contains data structures and constant definition is supposed to be used
**	in the core AVProto routines as well as in general API routines.
**
**  DESIGN ISSUE:
**	This API - is a set of routines to performs encapsulation data into the TVL containers
**	and extract data.
**
**	See example of using this API at end of this module!
**
**
**  SET OF ROUTINES:
**
**	int	__tlv$hset (void *pdu, char *sig, unsigned u_csr, unsigned u_seq);
**		- Initialize PDU with given values, reset PDU's length field to initial state.
**
**
**	int	__tlv$hget (void *pdu, char *sig, unsigned *u_len, unsigned *u_csr, unsigned *u_seq);
**		- Retrieve values from the PDU
**
**	int	__tlv$put  (void *pdu, unsigned short pdusz, unsigned v_tag, unsigned v_type, void *val, unsigned valsz);
**		- Add new TLV into the PDU (at end);
**
**	int	__tlv$get  (void *pdu, [int *context], unsigned v_tag, unsigned *v_type, void *val, [unsigned *valsz]);
**		- Retrieve TLV from the given PDU
**
**	int	__tlv$lookup  (void *pdu, [int *context], unsigned v_tag, unsigned *v_type, void **val, [unsigned *valsz]);
**		- Lookup TLV with specified Tag Id (v_tag), return an address of the value (unconverted !)  in the PDU's area and size.
**
**
**	unsigned short __tlv$encode_tag( unsigned short v_type, unsigned short v_tag);
**		- Encapsulate Tag Id and Value Type into the 16-bits variable;
**
**	unsigned short __tlv$decode_tag (unsigned short w_tag, unsigned short	*v_type, unsigned short	*v_tag);
**		- Decode Tag Id and Value Type from the 16-bits opaque;
**
**
**   USAGE:
**		#include	"avproto.h"
**		add avproto.c module into project
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  20-JUL-2018
**
**  MODIFICATION HISTORY:
**
**	14-AUG-2026	RRL	X.94-11ECO1 : the mandatory __REV__ was absent from the module
**		header; per the Xaa-bbECOc -> aa.bb.c rule it is "94.11.1" (this very
**		addition being the ECO).
**
**	 6-FEB-2019	SYS	Some redesigning according changes in the AVP's structure.
**
**	15-FEB-2019	RRL	Redesigned to exclude using bitfields ...
**
**	18-FEB-2019	RRL	Added API routine to lookup functionality.
**
**	22-FEB-2019	RRL	X.94-02 : Added destination buffer checking in the __tlv$get();
**				moved __tlv$hget()/__tlv$hset() to .H;
**				unhardcoded "Z0magic" signature - now it's parameter;
**
**	25-FEB-2019	SYS	X.94-03 : Fixed bug with ptlv pointer computation in the __tlv$get(), __tlv$lookup()
**
**	26-FEB-2019	SYS	X.94-04 : Recoded to make 'context' parameter is optional in the __tlv$get()/__tlv$lookup()
**
**	 6-JUN-2019	RRL	X.94-05 : Added zeroing area before copying  attribute value in the __tlv$put()
**
**	 2-JUL-2019	RRL	X.94-06 : Parameter 'valsz' of the __tlv$put() is optional now for fixed size arguments (WORD, LONGWORD, QUADWORD, UUID)
**
**	29-JUL-2019	RRL	X.94-07 : Redeclared __tlv$dump();
**
**	27-JUL-2026	RRL	X.94-08 : Security/robustness fixes:
**				- __tlv$get()/__tlv$lookup()/__tlv$dump() take real PDU area
**				  size <pdusz> and validate every TLV against it (no OOB on a
**				  malformed/hostile PDU with a lying u_len/w_len);
**				- fixed 'next TLV' pointer computation in __tlv$lookup() (was
**				  based on <pdu> instead of <ptlv>), set returned value length;
**				- __tlv$get(): require <valsz> for variable-length tags, clamp
**				  UUID copy to the destination buffer;
**				- __tlv$put(): fixed unsigned-short underflow of the free-space
**				  computation.
**
**
**
**	28-JUL-2026	RRL	X.94-09 : Renamed the routine prefix avproto_ to __tlv$ (the module
**				implements a Tag-Length-Value protocol). Uppercase types
**				AVPROTO_PDU/AVPROTO_HDR/AVPROTO_TLV are unchanged.
**
**--
*/

#include	<stdint.h>
#include	<stddef.h>
#include	<ctype.h>
#include	<limits.h>
#include	<stdarg.h>
#include	<string.h>
#include	<endian.h>

#include	"utility_routines.h"
#include	"avproto.h"	/* Constants and data structure definitions */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Append one Tag-Length-Value record to a PDU. Word, longword and quadword
**	values are stored in network byte order; the header payload length is
**	updated. Fails if the value does not fit in the remaining buffer space.
**
**  FORMAL PARAMETERS:
**
**	a_pdup	Address of the PDU buffer
**	a_pdusz	Total size of the PDU buffer in octets
**	a_v_tag	Application tag identifier
**	a_v_type	Tag type (TAG$K_*)
**	a_val	Address of the value to store
**	a_valsz	Size of the value in octets
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Updates the length field in the PDU header.
**
**  RETURN VALUE:
**	STS$K_SUCCESS on success; error if the buffer is too small or the header
**	length is corrupt.
**--
*/
int	__tlv$put (
	void *		a_pdup,
	unsigned short	a_pdusz,
	unsigned	a_v_tag,
	unsigned	a_v_type,
	void	*	a_val,
	unsigned	a_valsz
		)
{
unsigned	l_len = 0, l_used, l_free;
char	*	l_pchar;
AVPROTO_TLV *	l_ptlv;
AVPROTO_PDU *	l_pdu = (AVPROTO_PDU *) a_pdup;

	/* The PDU area must hold at least the fixed header */
	if ( a_pdusz < sizeof(AVPROTO_HDR) )
		return	$LOG(STS$K_ERROR, "PDU area too small: %u octets", (unsigned) a_pdusz);

	/* Get current PDU's payload size from the header */
	l_used = be32toh(l_pdu->r_hdr.u_len);

	/* A declared payload can not be larger than the given PDU area (no unsigned underflow) */
	if ( l_used > (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)) )
		return	$LOG(STS$K_ERROR, "Corrupted PDU: payload=%u > area=%u", l_used, (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)));

	/* Free space left in the PDU's payload area */
	l_free = (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)) - l_used;

	/* Compute pointer to "first free byte" of the PDU's area */
	l_pchar = (char *) l_pdu->r_tlv + l_used;

	/* Check for free space in the PDU's area */
	if ( (sizeof (AVPROTO_TLV) + a_valsz) > l_free )
		return	$LOG(STS$K_ERROR, "No free space for tag=%d, type=%d, len=%d", a_v_tag, a_v_type, a_valsz);

	/* Set TLV pointer to end of the current PDU  */
	l_ptlv = (AVPROTO_TLV *) (l_pchar);

	/* Pack Tag type and Tag id into the field */
	l_ptlv->w_tag = __tlv$encode_tag (a_v_type, a_v_tag);

	/* Put value into the TLV container */
	switch ( a_v_type )
		{
		case	TAG$K_WORD:	/* 16 bits */
			{
			uint16_t l_v = 0;				/* unaligned-safe: b_val may sit at an odd offset */

			memcpy(&l_v, a_val, a_valsz ? (size_t) $MIN(a_valsz, sizeof(l_v)) : sizeof(l_v));
			l_v = htobe16(l_v);
			memcpy(l_ptlv->b_val, &l_v, sizeof(l_v));

			l_ptlv->w_len = htobe16( l_len = sizeof(uint16_t) );

			break;
			}

		case	TAG$K_LONGWORD:	/* 32 bits */
			{
			uint32_t l_v = 0;

			memcpy(&l_v, a_val, a_valsz ? (size_t) $MIN(a_valsz, sizeof(l_v)) : sizeof(l_v));
			l_v = htobe32(l_v);
			memcpy(l_ptlv->b_val, &l_v, sizeof(l_v));

			l_ptlv->w_len = htobe16( l_len = sizeof(uint32_t) );

			break;
			}

		case	TAG$K_QWORD:	/* 64 bits */
			{
			uint64_t l_v = 0;

			memcpy(&l_v, a_val, a_valsz ? (size_t) $MIN(a_valsz, sizeof(l_v)) : sizeof(l_v));
			l_v = htobe64(l_v);
			memcpy(l_ptlv->b_val, &l_v, sizeof(l_v));

			l_ptlv->w_len = htobe16( l_len = sizeof(uint64_t) );

			break;
			}

		case	TAG$K_UUID:	/* 16 octets  */
			memset(l_ptlv->b_val, 0, 16);

			memcpy(l_ptlv->b_val, a_val, l_len = $MIN(a_valsz, 16));
			l_ptlv->w_len = htobe16( 16 );

			break;

		default:
			memcpy(l_ptlv->b_val, a_val, l_len = a_valsz);
			l_ptlv->w_len = htobe16( a_valsz );

			break;
		}

	/* Adjust PDU's length  */
	l_pdu->r_hdr.u_len = htobe32(sizeof (AVPROTO_TLV) + l_len + be32toh(l_pdu->r_hdr.u_len) );

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Find a TLV by tag and copy its value into a caller buffer. Word, longword
**	and quadword values are converted from network to host byte order. Every
**	record is validated against <a_pdusz> so a malformed PDU cannot overrun.
**
**  FORMAL PARAMETERS:
**
**	a_pdup	Address of the received PDU
**	a_pdusz	REAL size of the PDU area (received octets)
**	a_context	Iteration context; -1 on the first call
**	a_v_tag	Tag identifier to find
**	a_v_type	Receives the tag type
**	a_val	Buffer that receives the value
**	a_valsz	In: size of <a_val>; out: octets copied (mandatory for var-length)
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Advances <a_context> through the PDU.
**
**  RETURN VALUE:
**	STS$K_SUCCESS if found and copied; STS$K_WARN if absent; error if the
**	destination is too small or <a_valsz> was omitted for a variable-length type.
**--
*/
int	__tlv$get (
		void	*a_pdup,
	unsigned	a_pdusz,		/* Real size of the PDU area (received octets) */
	int	*	a_context,
	unsigned	a_v_tag,
	unsigned	*a_v_type,
	void	*	a_val,
	unsigned *	a_valsz
		)
{
int		l_status, l_pos = 0;
unsigned	l_len = 0, l_limit;
unsigned short	l_wlen = 0, l_tag = 0, l_type = 0;
AVPROTO_TLV *	l_ptlv;
AVPROTO_PDU *	l_pdu = (AVPROTO_PDU *) a_pdup;

	/* The PDU area must hold at least the fixed header */
	if ( a_pdusz < sizeof(AVPROTO_HDR) )
		return	$LOG(STS$K_ERROR, "PDU area too small: %u octets", a_pdusz);

	/* Effective payload limit = min(declared by header, really available) - do not trust a lying header */
	l_limit = be32toh(l_pdu->r_hdr.u_len);
	l_limit = $MIN(l_limit, (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)));

	/* Compute a pointer to first unprocessed TLV */
	if ( a_context && (*a_context != -1) )
		l_ptlv = (AVPROTO_TLV *) (((char *) l_pdu->r_tlv) + (l_pos = *a_context));
	else	{
		l_ptlv = l_pdu->r_tlv;
		l_pos = 0;
		}

	/* Find TLV with a given Tag Id */
	for ( l_status = STS$K_WARN; (unsigned) l_pos < l_limit; )
		{
		/* The fixed TLV header itself must fit into the remaining area */
		if ( ((unsigned) l_pos + sizeof(AVPROTO_TLV)) > l_limit )
			break;

		__tlv$decode_tag (l_ptlv->w_tag, &l_type, &l_tag);
		l_wlen = be16toh(l_ptlv->w_len);

		/* The whole TLV (header + value) must fit too */
		if ( ((unsigned) l_pos + sizeof(AVPROTO_TLV) + l_wlen) > l_limit )
			break;

		/* Adjust context */
		l_pos += sizeof(AVPROTO_TLV) + l_wlen;

		/* Note: intentional assignment (not ==): l_status becomes 1 on a Tag Id match */
		if ( l_status = (a_v_tag == l_tag) )
			break;

		/* Jump to next TLV */
		l_ptlv = (AVPROTO_TLV *) (((char *) l_ptlv) + sizeof(AVPROTO_TLV) + l_wlen);
		}

	if ( a_context )
		*a_context = l_pos;


	/* Not found ? */
	if ( !(1 & l_status) )
		return	l_status;

	/*
	 * Destination buffer size check. For variable-length values (UUID, octet block, unknown)
	 * the caller MUST provide <a_valsz>, otherwise a buffer overrun can not be excluded.
	 */
	switch ( l_type )
		{
		case	TAG$K_WORD:
		case	TAG$K_LONGWORD:
		case	TAG$K_QWORD:
			if ( a_valsz && (*a_valsz) < l_wlen )
				return	STS$K_ERROR;
			break;

		default:
			if ( !a_valsz )
				return	$LOG(STS$K_ERROR, "valsz is required for variable-length tag type=%d", l_type);

			if ( (*a_valsz) < l_wlen )
				return	STS$K_ERROR;
		}

	/* Convert and extract value according the type */
	*a_v_type = l_type;


	switch ( l_type )
		{
		case	TAG$K_WORD:	/* 16 bits */
			{
			uint16_t l_v;				/* unaligned-safe read from a packed TLV */

			memcpy(&l_v, l_ptlv->b_val, sizeof(l_v));
			l_v = be16toh(l_v);
			memcpy(a_val, &l_v, sizeof(l_v));
			l_len  = sizeof(uint16_t);
			}
			break;

		case	TAG$K_LONGWORD:	/* 32 bits */
			{
			uint32_t l_v;

			memcpy(&l_v, l_ptlv->b_val, sizeof(l_v));
			l_v = be32toh(l_v);
			memcpy(a_val, &l_v, sizeof(l_v));
			l_len  = sizeof(uint32_t);
			}
			break;

		case	TAG$K_QWORD:	/* 64 bits */
			{
			uint64_t l_v;

			memcpy(&l_v, l_ptlv->b_val, sizeof(l_v));
			l_v = be64toh(l_v);
			memcpy(a_val, &l_v, sizeof(l_v));
			l_len  = sizeof(uint64_t);
			}
			break;

		case	TAG$K_UUID:	/* 16 octets  */
			{
			unsigned short l_dstlen = (unsigned short) $MIN(*a_valsz, 16);
			unsigned short l_srclen = $MIN(l_wlen, l_dstlen);

			__util$movc5 (&l_srclen, (char *) l_ptlv->b_val, 0, &l_dstlen, a_val);
			l_len = l_dstlen;
			}

			break;
		default:
			memcpy(a_val, l_ptlv->b_val, l_wlen);

			l_len = l_wlen;

			break;
		}

	if ( a_valsz )
		*a_valsz = l_len;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Find a TLV by tag and return a pointer to its value inside the PDU, with
**	no copy. The data is NOT byte-order converted. The pointer is valid only
**	while the caller's PDU buffer lives. Suitable for large octet blocks.
**
**  FORMAL PARAMETERS:
**
**	a_pdup	Address of the received PDU
**	a_pdusz	REAL size of the PDU area (received octets)
**	a_context	Iteration context; -1 on the first call
**	a_v_tag	Tag identifier to find
**	a_v_type	Receives the tag type
**	a_val	Receives a pointer to the value inside the PDU
**	a_valsz	Receives the length of the value
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Advances <a_context> through the PDU.
**
**  RETURN VALUE:
**	STS$K_SUCCESS if the record was found; STS$K_WARN otherwise.
**--
*/
int	__tlv$lookup (
		void	*a_pdup,
	unsigned	a_pdusz,		/* Real size of the PDU area (received octets) */
	int	*	a_context,
	unsigned	a_v_tag,
	unsigned	*a_v_type,
	void	**	a_val,
	unsigned *	a_valsz
		)
{
int		l_status, l_pos = 0;
unsigned	l_len = 0, l_limit;
unsigned short	l_wlen = 0, l_tag = 0, l_type = 0;
AVPROTO_TLV *	l_ptlv;
AVPROTO_PDU *	l_pdu = (AVPROTO_PDU *) a_pdup;

	/* The PDU area must hold at least the fixed header */
	if ( a_pdusz < sizeof(AVPROTO_HDR) )
		return	$LOG(STS$K_ERROR, "PDU area too small: %u octets", a_pdusz);

	/* Effective payload limit = min(declared by header, really available) */
	l_limit = be32toh(l_pdu->r_hdr.u_len);
	l_limit = $MIN(l_limit, (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)));

	/* Compute a pointer to first unprocessed TLV */
	if ( a_context && (*a_context != -1) )
		l_ptlv = (AVPROTO_TLV *) (((char *) l_pdu->r_tlv) + (l_pos = *a_context));
	else	{
		l_ptlv = l_pdu->r_tlv;
		l_pos = 0;
		}

	/* Find TLV with a given Tag Id */
	for ( l_status = STS$K_WARN; (unsigned) l_pos < l_limit; )
		{
		/* The fixed TLV header itself must fit into the remaining area */
		if ( ((unsigned) l_pos + sizeof(AVPROTO_TLV)) > l_limit )
			break;

		__tlv$decode_tag (l_ptlv->w_tag, &l_type, &l_tag);
		l_wlen = be16toh(l_ptlv->w_len);

		/* The whole TLV (header + value) must fit too */
		if ( ((unsigned) l_pos + sizeof(AVPROTO_TLV) + l_wlen) > l_limit )
			break;

		/* Adjust context */
		l_pos += sizeof(AVPROTO_TLV) + l_wlen;

		/* Note: intentional assignment (not ==): l_status becomes 1 on a Tag Id match */
		if ( l_status = (a_v_tag == l_tag) )
			break;

		/* Jump to next TLV - relative to the CURRENT TLV, not to the PDU base ! */
		l_ptlv = (AVPROTO_TLV *) (((char *) l_ptlv) + sizeof(AVPROTO_TLV) + l_wlen);
		}

	if ( a_context )
		*a_context = l_pos;

	/* Not found ? */
	if ( !(1 & l_status) )
		return	l_status;

	l_len = l_wlen;			/* Length of the found TLV's value block */

	*a_v_type = l_type;
	*a_val = l_ptlv->b_val;

	if ( a_valsz )
		*a_valsz = l_len;


	return	STS$K_SUCCESS;
}




/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write a human-readable listing of every TLV in a PDU to the log: each
**	record with its tag, type, length and a hex dump of the value. For debugging.
**
**  FORMAL PARAMETERS:
**
**	a_pdu	Address of the PDU
**	a_pdusz	REAL size of the PDU area (received octets)
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the current log destination.
**
**  RETURN VALUE:
**	NONE.
**--
*/
void	__tlv$dump	(
		void	*a_pdu,
		unsigned a_pdusz		/* Real size of the PDU area (received octets) */
			)
{
char		l_hexbuf1[512], l_hexbuf2[512];
char	*	l_pchar;
unsigned	l_limit, l_count, l_pos;
unsigned short	l_wlen, l_wtag, l_vtype, l_vtag;
AVPROTO_TLV *	l_ptlv;
AVPROTO_PDU *	l_pdu = (AVPROTO_PDU *) a_pdu;


	if ( a_pdusz < sizeof(AVPROTO_HDR) )
		{
		$LOG(STS$K_ERROR, "PDU area too small: %u octets", a_pdusz);
		return;
		}

	/* Effective payload limit = min(declared by header, really available) */
	l_limit = be32toh(l_pdu->r_hdr.u_len);
	l_limit = $MIN(l_limit, (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)));

	$LOG(STS$K_INFO, "PDU [len=%d, csr=%#x, seq=%d]", be32toh(l_pdu->r_hdr.u_len), be32toh(l_pdu->r_hdr.u_csr), be32toh(l_pdu->r_hdr.u_seq));
	/* Walk every TLV record in the PDU and print tag / type / length / value */

	for ( l_pchar = (char *) l_pdu->r_tlv, l_pos = l_count = 0; l_pos < l_limit; l_count++ )
		{
		/* The fixed TLV header must fit */
		if ( (l_pos + sizeof(AVPROTO_TLV)) > l_limit )
			break;

		l_ptlv  = (AVPROTO_TLV *) l_pchar;
		l_wlen = be16toh(l_ptlv->w_len);
		l_wtag = __tlv$decode_tag (l_ptlv->w_tag, &l_vtype, &l_vtag);

		/* The whole TLV must fit */
		if ( (l_pos + sizeof(AVPROTO_TLV) + l_wlen) > l_limit )
			{
			$LOG(STS$K_ERROR, "[%04.4d] truncated TLV [tag=%04x, len=%u] at offset %u", l_count, l_wtag, l_wlen, l_pos);
			break;
			}

		__util$bin2hex (l_ptlv, l_hexbuf1, sizeof(AVPROTO_TLV));
		__util$bin2hex (l_ptlv->b_val, l_hexbuf2, $MIN(l_wlen, sizeof(l_hexbuf2)/2) );
		$LOG(STS$K_INFO, "[%04.4d] TLV [tag=%04x(id=%04x, type=%04x), len=%02d] 0x%s:0x%s",
		     l_count, l_wtag, l_vtag, l_vtype, l_wlen, l_hexbuf1, l_hexbuf2);

		l_pos   += sizeof(AVPROTO_TLV) + l_wlen;
		l_pchar += sizeof(AVPROTO_TLV) + l_wlen;
		}
}


//#define	__AVPROTO_MAIN__	1
#if	__AVPROTO_MAIN__	/* At development/debug time	*/



const char VCLOUD$K_PROTOSIG [] = "Z0magic";


/* A set of definitions for toy client-server interoperations	*/
enum	{
	VCLOUD$K_CMD_LOGIN,
	VCLOUD$K_CMD_LOGOUT,
	VCLOUD$K_CMD_GET
};

enum	{
	VCLOUD$K_STS_ERROR,
	VCLOUD$K_STS_SUCCESS
};


/* An example set of tag's id for demonstration purpose */
enum	{
	VCLOUD$K_TAG_RESULT = 1,
	VCLOUD$K_TAG_COMP_ID,	/* Computer Id	*/
	VCLOUD$K_TAG_DISK_ID,	/* Disk Id	*/
	VCLOUD$K_TAG_KEY = 4,	/* Key Block */
	VCLOUD$K_TAG_UNAME,
	VCLOUD$K_TAG_PASS,
	VCLOUD$K_TAG_MSG
};

char	pdubuf[8192];	/* This buffer will keep the PDU is shared between
			  toy client and server */

AVPROTO_PDU *pdu = (AVPROTO_PDU *) pdubuf;


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__AVPROTO_MAIN__ demo harness - not part of the library API.]
**
**	Demo TLS client: build an SSL context, connect to the server, then
**	exchange a request/response PDU pair over the encrypted channel.
**
**  FORMAL PARAMETERS:
**
**	NONE
**
**  RETURN VALUE:
**	A condition code (STS$K_*).
**--
*/
int	__client	(void)
{
unsigned char	username[] = "Ruslan (BadAss SysMan) Laishev",
		password[] = "My cool password string";

unsigned short	w_val = 0x1234;
unsigned 	u_val = 0x1234abcd;
long long	q_val = 0x1234abcddeadbeef;

	/* Initialize header of the PDU */
	__tlv$hset (pdu, VCLOUD$K_PROTOSIG, /* Command */ VCLOUD$K_CMD_LOGIN, /* Sequence */ 17);

	/* Encapsulate some data into the PDU */
	__tlv$put (pdu, sizeof(pdubuf),	/* PDU'address and maximum size of the PDU's area */
		     VCLOUD$K_TAG_UNAME,	/* 'Tag Id' constant				*/
		     0,				/* Default 'Tag Type' is binary data block	*/
		     username,			/* Address of the 'Value', length ...		*/
		     sizeof(username) - 1);

	__tlv$put (pdu, sizeof(pdubuf), VCLOUD$K_TAG_PASS, 0, password, sizeof(password) - 1);
	__tlv$put (pdu, sizeof(pdubuf), TAG$K_WORD, TAG$K_WORD, &w_val, sizeof(w_val) );
	__tlv$put (pdu, sizeof(pdubuf), TAG$K_LONGWORD, TAG$K_LONGWORD, &u_val, sizeof(u_val) );
	__tlv$put (pdu, sizeof(pdubuf), TAG$K_QWORD, TAG$K_QWORD, &q_val, sizeof(q_val) );

	return	STS$K_SUCCESS;
}

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__AVPROTO_MAIN__ demo harness - not part of the library API.]
**
**	Demo TLS server: build an SSL context, listen and accept a connection,
**	then read a request PDU and answer with a response PDU.
**
**  FORMAL PARAMETERS:
**
**	NONE
**
**  RETURN VALUE:
**	A condition code (STS$K_*).
**--
*/
int	__server	(void)
{
unsigned context = -1, u_len, u_csr, u_seq, v_type, mlen;
char	uname[128], pass [128];
unsigned short ulen, plen, w_val = 0;
unsigned u_val = 0;
unsigned long long q_val = 0;
char	welcome [] = "This is an answer to request: cmd/seq=%u/%u",
	msg [ 512 ];


	/* Get header's values of the PDU */
	if ( !(1 & __tlv$hget (pdu, VCLOUD$K_PROTOSIG,  &u_len, &u_csr, &u_seq)) )
		return	$LOG(STS$K_ERROR, "Malformed or corrupted PDU");

	/* Retrieve attributes from the request */
	ulen = sizeof(uname);
	__tlv$get (pdu, sizeof(pdubuf), &context, VCLOUD$K_TAG_UNAME, &v_type, uname, &ulen);
	$TRACE("uname=[0:%d]='%.*s'", ulen , ulen, uname);

	plen = sizeof(pass);
	__tlv$get (pdu, sizeof(pdubuf), &context, VCLOUD$K_TAG_PASS, &v_type, pass, &plen);
	$TRACE("pass=[0:%d]='%.*s'", plen , plen, pass);

	__tlv$get (pdu, sizeof(pdubuf), &context, /* Is used as Tag Id */ TAG$K_WORD, &v_type, &w_val, 0 );
	$TRACE("w_val=%d/%#x", w_val, w_val);

	__tlv$get (pdu, sizeof(pdubuf), &context, /* Is used as Tag Id */ TAG$K_LONGWORD, &v_type, &u_val, 0 );
	$TRACE("u_val=%d/%#x", u_val, u_val);

	__tlv$get (pdu, sizeof(pdubuf), &context, /* Is used as Tag Id */ TAG$K_QWORD, &v_type, &q_val, 0 );
	$TRACE("q_val=%llu/%#llx", q_val, q_val);

	/* Prepare answer to the request */
	__tlv$hset (pdu, VCLOUD$K_PROTOSIG, /* Status */ VCLOUD$K_STS_SUCCESS, /* Sequence */ u_seq);

	/* Encapsulate text message into the PDU */
	mlen = sprintf(msg, welcome, u_csr, u_seq);
	__tlv$put (pdu, sizeof(pdubuf),	/* PDU'address and maximum size of the PDU's area */
		     VCLOUD$K_TAG_MSG,		/* 'Tag Id' constant				*/
		     0,				/* Default 'Tag Type' is binary data block	*/
		     msg,			/* Address of the 'Value', length ...		*/
		     mlen);

	return	STS$K_SUCCESS;

}

#if	0
void	main	(void)
{

	/* Form a request ... */
	__client();

	$DUMPHEX(pdu, be32toh(pdu->r_hdr.u_len));
	__tlv$dump(pdu);


	/* Process request from client - form the answer */
	__server();

	$DUMPHEX(pdu, be32toh(pdu->r_hdr.u_len));
	__tlv$dump(pdu);

}
#endif




#if	1
#include	<stdio.h>
#include	<stdlib.h>
#include	<stdbool.h>
#include	<string.h>
#include	<unistd.h>
#include	<netinet/ip.h>
#include	<arpa/inet.h>
#include	<netdb.h>
#include	<fcntl.h>
#include	<poll.h>
#include	<openssl/ssl.h>
#include	<openssl/err.h>
#include	<uuid/uuid.h>

#include	"vcloud_common.h"

const char	clntcert_fname [] = "vCloud-client.crt",
		clntkey_fname [] = "vCloud-client.key",
		cacert_fname [] =  "rootCA.crt";

#define	VCLOUD$K_IOTMO	13

/*
 *   DESCRIPTION: Read n bytes from the network socket, wait if not all data has been get
 *		but no more then 13 seconds.
 *
 *   INPUT:
 *	sd:	Network socket descriptor
 *	ssl:	SSL' session context
 *	buf:	A buffer to accept data
 *	bufsz:	A number of bytes to be read
 *
 *  OUTPUT:
 *	buf:	Received data
 *
 *  RETURN:
 *	condition code, see STS$K_* constant
 */
inline	static int __recv_n
		(
		int	sd,
		SSL	*ssl,
	unsigned char	*buf,
		int	bufsz
		)
{
int	status, restbytes = bufsz, tmo = VCLOUD$K_IOTMO;
struct pollfd pfd = {sd, POLLIN, 0};

	for ( restbytes = bufsz; (tmo--) && restbytes; )
		{
		if ( !SSL_pending(ssl) ) /* Is the internal SSL' buffer empty ?*/
			{
			if( 0 >  (status = poll(&pfd, 1, 1000)) && (errno != EINTR) )
				return	$LOG(STS$K_ERROR, "[#%d] poll/select() -> %d, errno=%d, requested %d octets, rest %d octets", sd, status, errno, bufsz, restbytes);
			else if ( (status < 0) && (errno == EINTR) )
				{
				$LOG(STS$K_WARN, "[#%d] poll/select() -> %d, errno=%d, requested %d octets, rest %d octets", sd, status, errno, bufsz, restbytes);
				continue;
				}

			if ( pfd.revents & (~POLLIN) )	/* Unexpected events ?!			*/
				return	$LOG(STS$K_FATAL, "[#%d] poll() -> %d, .revents=%08x(%08x), errno=%d",
						sd, status, pfd.revents, pfd.events, errno);

			if ( !(pfd.revents & POLLIN) )	/* Non-interesting event ?		*/
				continue;
			}

		/* Do we reach an end of I/O time ?	*/
		if ( tmo <= 0 )
			{
			$LOG(STS$K_ERROR, "[#%d] Timeout (%d seconds) has been reached", sd, VCLOUD$K_IOTMO);
			break;
			}

		/* Retrieve data from socket buffer	*/
		if ( restbytes == (status = SSL_read(ssl, buf, restbytes)) )
			{
			$DUMPHEX(buf, bufsz);
			return	STS$K_SUCCESS; /* Bingo! We has been received a requested amount of data */
			}

		if ( 0 >= status )
			{
			$LOG(STS$K_ERROR, "[#%d] recv(%d octets) -> %d, .revents=%08x(%08x), errno=%d",
					sd, restbytes, status, pfd.revents, pfd.events, errno);
			break;
			}

		/* Advance buffer pointer and decrease expected byte counter */
		restbytes -= status;
		buf	+= status;
		}

	return	$LOG(STS$K_ERROR, "[#%d] Did not get requested %d octets in %d seconds, rest %d octets", sd, bufsz, tmo, restbytes);
}


/*
 *   DESCRIPTION: Write n bytes to the network socket, wait if not all data has been get
 *		but no more then 13 seconds;
 *
 *   INPUT:
 *	sd:	Network socket descriptor
 *	ssl:	SSL' session context
 *	buf:	A buffer with data to be sent
 *	bufsz:	A number of bytes to be read
 *
 *  OUTPUT:
 *	NONE
 *
 *  RETURN:
 *	condition code, see STS$K_* constant
 */
inline	static int __xmit_n
		(
		int	sd,
		SSL	*ssl,
	unsigned char	*buf,
		int	bufsz
		)
{
int	status, restbytes = bufsz, tmo = VCLOUD$K_IOTMO;
struct pollfd pfd = {sd, POLLOUT, 0};

	$DUMPHEX(buf, bufsz);

	for ( restbytes = bufsz; (tmo--) && restbytes; )
		{
		if( 0 >  (status = poll(&pfd, 1, 1000)) && (errno != EINTR) )
			return	$LOG(STS$K_ERROR, "[#%d] poll/select() -> %d, errno=%d, requested %d octets, rest %d octets", sd, status, errno, bufsz, restbytes);
		else if ( (status < 0) && (errno == EINTR) )
			{
			$LOG(STS$K_WARN, "[#%d] poll/select() -> %d, errno=%d, requested %d octets, rest %d octets", sd, status, errno, bufsz, restbytes);
			continue;
			}

		if ( pfd.revents & (~POLLOUT) )	/* Unexpected events ?!			*/
			return	$LOG(STS$K_FATAL, "[#%d] poll() -> %d, .revents=%08x(%08x), errno=%d",
					sd, status, pfd.revents, pfd.events, errno);

		if ( !(pfd.revents & POLLOUT) )	/* No interesting event			*/
			continue;

		/* Send data to socket buffer	*/
		if ( restbytes == (status = SSL_write(ssl, buf, restbytes)) )
			return	STS$K_SUCCESS; /* Bingo! We has been sent a requested amount of data */

		if ( 0 >= status )
			{
			$LOG(STS$K_ERROR, "[#%d] send(%d octets) -> %d, .revents=%08x(%08x), errno=%d",
					sd, restbytes, status, pfd.revents, pfd.events, errno);
			break;
			}

		/* Advance buffer pointer and decrease to be sent byte counter */
		restbytes -= status;
		buf	+= status;
		}

	return	$LOG(STS$K_ERROR, "[#%d] Did not put requested %d octets in %d seconds, rest %d octets", sd, bufsz, VCLOUD$K_IOTMO, restbytes);
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__AVPROTO_MAIN__ demo harness - not part of the library API.]
**
**	Read exactly one PDU from a TLS-protected socket: first the fixed header,
**	then the payload whose length the header declares.
**
**  FORMAL PARAMETERS:
**
**	sd	Connected socket descriptor
**	ssl	OpenSSL connection object
**	pdubuf	Buffer that receives the PDU
**	pdusz	Capacity of <pdubuf> in octets
**
**  RETURN VALUE:
**	A condition code (STS$K_*).
**--
*/
int	pdu_recv	(
		int	sd,
		SSL	*ssl,
		void	*pdubuf,
	unsigned	 pdusz
			)

{
unsigned status, u_len, u_csr, u_seq;

	if ( pdusz < sizeof(AVPROTO_HDR) )
		return	$LOG(STS$K_ERROR, "No free space to read PDU's header");

	/* Get PDU header */
	if ( !(1 & (status = __recv_n (sd, ssl, pdubuf, sizeof(AVPROTO_HDR)))) )
	     return	$LOG(STS$K_ERROR, "Error receiving PDU's header");

	/* Check magic, extract PDU's Length, CSR, Sequence ... */
	if ( !(__tlv$hget (pdubuf, VCLOUD$K_PROTOSIG, &u_len, &u_csr, &u_seq)) )
		return	$LOG(STS$K_ERROR, "Malformed or invalid PDU's header");

	if ( pdusz < (sizeof(AVPROTO_HDR) + u_len) )
		return	$LOG(STS$K_ERROR, "No free space to read PDU's body (len=%d, csr=%d, seq=%d)", u_len, u_csr, u_seq);

	/* Read rest of the PDU */
	if ( !(1 & (status = __recv_n (sd, ssl, pdubuf + sizeof(AVPROTO_HDR), u_len))) )
		return	$LOG(STS$K_ERROR, "Error reading PDU's body (len=%d, csr=%d, seq=%d)", u_len, u_csr, u_seq);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__AVPROTO_MAIN__ demo harness - not part of the library API.]
**
**	Write one complete PDU (header plus payload) to a TLS-protected socket.
**
**  FORMAL PARAMETERS:
**
**	sd	Connected socket descriptor
**	ssl	OpenSSL connection object
**	pdubuf	The PDU to send
**
**  RETURN VALUE:
**	A condition code (STS$K_*).
**--
*/
int	pdu_xmit	(
		int	sd,
		SSL	*ssl,
		void	*pdubuf
			)

{
unsigned status, u_len, u_csr, u_seq;

	/* Check magic, extract PDU's Length, CSR, Sequence ... */
	if ( !(__tlv$hget (pdubuf, VCLOUD$K_PROTOSIG,  &u_len, &u_csr, &u_seq)) )
		return	$LOG(STS$K_ERROR, "Malformed or invalid PDU's header");

	/* Send the PDU */
	if ( !(1 & (status = __xmit_n (sd, ssl, pdubuf, sizeof(AVPROTO_HDR) + u_len))) )
		return	$LOG(STS$K_ERROR, "Error transmitting PDU's (len=%d, csr=%d, seq=%d)", u_len, u_csr, u_seq);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__AVPROTO_MAIN__ demo harness - not part of the library API.]
**
**	Create and configure an OpenSSL context for the demo: pick the method,
**	load the certificate and private key, and set the verification mode.
**
**  FORMAL PARAMETERS:
**
**	__ctx	Receives the newly created SSL_CTX
**
**  RETURN VALUE:
**	A condition code (STS$K_*).
**--
*/
int	init_ssl (SSL_CTX ** __ctx )
{
const SSL_METHOD *method = NULL;

	$TRACE("Initialize client SSL context ...");

	/* General initialization ...	*/
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	method = SSLv23_client_method();

	if ( (*__ctx = SSL_CTX_new(method)) )
		{
		$TRACE("Loading certificate (-s) from %s file ...", clntcert_fname);

		if ( 0 < SSL_CTX_use_certificate_file(*__ctx, clntcert_fname, SSL_FILETYPE_PEM)  )
			if ( 0 < SSL_CTX_use_PrivateKey_file(*__ctx, clntkey_fname, SSL_FILETYPE_PEM) )
				{
				if ( SSL_CTX_check_private_key(*__ctx) )
					{
					/* Load trusted CA. */
					if ( SSL_CTX_load_verify_locations(*__ctx, cacert_fname, NULL) )
						{
						SSL_CTX_set_verify(*__ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
						SSL_CTX_set_verify_depth(*__ctx, 4);
						return	STS$K_SUCCESS;
						}
					}
				else	$LOG(STS$K_FATAL, "Private key  does not match the public certificate");
				}
		}

	ERR_print_errors_fp(stderr);

	return	$LOG(STS$K_FATAL, "Error initialization of SSL stuff");
}


#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__AVPROTO_MAIN__ demo harness - not part of the library API.]
**
**	Fetch a drive key from a KDEPO (key-depot) server: connect, send the
**	computer UUID and receive the key block for each drive.
**
**  FORMAL PARAMETERS:
**
**	rsock	Address of the KDEPO server socket
**	cuu	Computer UUID identifying the caller
**	dk	Array that receives the returned drive keys
**
**  RETURN VALUE:
**	A condition code (STS$K_*).
**--
*/
int	kdepo_get_key	(
	struct sockaddr_in *	rsock,
		uuid_t		cuu,
		Z0_DRIVE_KEY *	dk
			)
{
int	sd = 0, status = 0, i, v_type = 0, valsz;
SSL *ssl = NULL;
SSL_CTX *ssl_ctx;
Z0_DRIVE_KEY *pdk;

	if ( !(1 & init_ssl(&ssl_ctx)) )
		return	$LOG(STS$K_FATAL, "Error initialization client SSL context");

	$LOG(STS$K_INFO, "Connecting to %s:%d", inet_ntoa(rsock->sin_addr), ntohs(rsock->sin_port));

	if ( 0 > (sd = socket(AF_INET, SOCK_STREAM, 0)) )
		return	$LOG(STS$K_ERROR, "socket() -> %d, errno = %d", sd, errno);

	if ( 0 > (status = connect(sd, (struct sockaddr *) rsock, sizeof(struct sockaddr))) )
		return	$LOG(STS$K_ERROR, "connect (%s:%d) -> %d, errno = %d",
			     inet_ntoa(rsock->sin_addr), ntohs(rsock->sin_port), status, errno);


	if ( 0 > (status = setsockopt( sd, IPPROTO_TCP, TCP_NODELAY, (void *)&sd, sizeof(sd))) )
		$LOG(STS$K_ERROR, "setsockopt()->%d, errno=%d", status, errno);

	/* We have etsablished the TCP-connection, so try to establishing SSL session */
	ssl = SSL_new(ssl_ctx);
	SSL_set_fd(ssl, sd);

	if ( 0 > SSL_connect(ssl) )
		{
		ERR_print_errors_fp(stderr);
		goto end;
		}

	/* Initialize header of the PDU */
	__tlv$hset (pdu, VCLOUD$K_PROTOSIG, /* Command */ VCLOUD$K_CMD_GET, /* Sequence */ 17);

	__tlv$put (pdu, sizeof(pdubuf), VCLOUD$K_TAG_COMP_ID, TAG$K_UUID, cuu, sizeof(uuid_t) );

	for (i = 0, pdk = dk; (i < DRIVES_MAX); i++, pdk++ )
		{
		if ( __util$iszero (pdk->disk_id, sizeof(pdk->disk_id)) )
			continue;

		__tlv$put (pdu, sizeof(pdubuf), VCLOUD$K_TAG_DISK_ID, TAG$K_UUID, pdk->disk_id, sizeof(pdk->disk_id) );
		}

	__tlv$dump (pdu);

	if ( !(1 & pdu_xmit (sd, ssl, pdu)) )
		$LOG(STS$K_ERROR, "Error sending request to Keys Depo");
	else if ( !(1 & pdu_recv (sd, ssl, pdu, sizeof(pdubuf))) )
		$LOG(STS$K_ERROR, "Error receiving answer from Keys Depo");
	else	{
		__tlv$dump (pdu);

		/* Processing answer ... */
		i = 0;

		if ( !(1 & (status = __tlv$get (pdu, NULL, VCLOUD$K_TAG_RESULT, &v_type, &i, NULL))) )
			status = $LOG(STS$K_ERROR, "Answer did not contains result code");

		if ( i != 1 )
			status = $LOG(STS$K_ERROR, "Error processing request, relt code=%#x", i);
		else	{
			valsz = sizeof(Z0_DRIVE_KEY);
			if ( !(1 & (status = __tlv$get (pdu, NULL, VCLOUD$K_TAG_KEY, &v_type, dk, &valsz))) )
				status = $LOG(STS$K_ERROR, "Answer did not contains DK");
			}
		}

end:
	if (ssl)
		{
		SSL_shutdown(ssl);
		SSL_free(ssl);
		}

	if (sd)
		close(sd);

	return	status;
}



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	[__AVPROTO_MAIN__ demo harness - not part of the library API.]
**
**	Entry point of the demo: obtain the drive keys from the KDEPO server
**	and print them, exercising the TLV and TLS helpers above.
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
int	main	(int argc, char ** argv)
{
int	status, i;
uuid_t	cuu;
Z0_DRIVE_KEY dk[DRIVES_MAX] = {0};
struct sockaddr_in rsock = {0};
char	buf[512];

	/* Convert test data to the binary form */
#if 1
	status =  uuid_parse( "1d75bd4a-0dbf-4d4d-9d5e-e24b29fdf1f7" , cuu);
	status =  uuid_parse( "c245486d-de9c-4abc-bdb1-e07f917e5ad8" , dk[0].disk_id);
	status =  uuid_parse( "368b1e4c-eff9-48d5-a085-0da2013f40ad" , dk[1].disk_id);

	inet_pton(AF_INET, "172.28.5.30", &rsock.sin_addr);
#endif
#if 0
	{
	char	cid [] = {"e62bbc999f7b364395a3b5e55c55adb0"},
		did [] = {"ad030abe877bc946b199f904e57a58d1"};


	__util$hex2bin(cid, &cuu, strlen(cid));
	__util$hex2bin(did, dk[0].disk_id, strlen(dk[0].disk_id));

	}
	inet_pton(AF_INET, "172.28.4.135", &rsock.sin_addr);
#endif

	/* Keys Depo host socket */
	rsock.sin_family = AF_INET;
	rsock.sin_port = htobe16(4343);


	/* Request keys ...*/
	if ( 1 & (status = kdepo_get_key(&rsock, cuu, dk)) )
		{
		/* Dump secret keys to the SYS$OUTPUT - suprize mutafaqa! */
		for (i = 0; i < DRIVES_MAX; i++ )
			{
			__util$bin2hex (dk[i].disk_id, buf, sizeof(dk[i].disk_id));
			$LOG(STS$K_INFO, "Disk Id [%02.2d]=0x%s", i, buf);

			__util$bin2hex (dk[i].key, buf, sizeof(dk[i].key));
			$LOG(STS$K_INFO, "Disk Key[%02.2d]=0x%s", i, buf);
			}
		}

}

#endif



#endif	/* #if	__AVPROTO_MAIN__ */
