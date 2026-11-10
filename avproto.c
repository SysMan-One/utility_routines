#define	__MODULE__	"AVPROTO"
#define	__IDENT__	"X.94-12ECO1"
#define	__REV__		"94.12.1"


/*
**++
**
**  FACILITY:  AVProto - Attribute/Value Pair (Tag Length Value) based protocol
**
**  ABSTRACT: Routines to put data into a PDU as TLV records (__tlv$hset/__tlv$hget/__tlv$put) and to extract it
**	(__tlv$get, __tlv$lookup, __tlv$dump); the tag id (12 bits) and the value type (4 bits) share one 16-bit word.
**	Every reader takes the REAL size of the PDU area and validates each record against it: a hostile PDU gives
**	an error status, never an overrun.  An example of the use is the __AVPROTO_MAIN__ harness at the end of the module.
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  20-JUL-2018
**
**  MODIFICATION HISTORY:
**
**	31-OCT-2026	CLD	X.94-12ECO1 : StarLet 1.7.1: __tlv$lookup() checks the wire length of a fixed-size record (WORD 2, LONGWORD 4, QWORD 8, UUID 16) as __tlv$get() does.
**
**	31-OCT-2026	CLD	X.94-12 : StarLet 1.7.0 hostile-input hardening: __tlv$put() checks free space by the real record length and the tag id
**				range, __tlv$get()/__tlv$lookup() check the wire length and report a truncated record as an error, __tlv$dump() in 64-octet chunks.
**
**	22-OCT-2026	RRL	X.94-11ECO2 : <endian.h> comes through avproto.h only (Windows has none; MinGW-w64 cross build gets _byteswap_*() stand-ins).
**
**	14-AUG-2026	RRL	X.94-11ECO1 : the mandatory __REV__ was absent from the module header; per the Xaa-bbECOc -> aa.bb.c rule it is "94.11.1".
**
**	 6-FEB-2019	SYS	Some redesigning according changes in the AVP's structure.
**
**	15-FEB-2019	RRL	Redesigned to exclude using bitfields ...
**
**	18-FEB-2019	RRL	Added API routine to lookup functionality.
**
**	22-FEB-2019	RRL	X.94-02 : Destination buffer checking in __tlv$get(); __tlv$hget()/__tlv$hset() moved to .H; the "Z0magic" signature is a parameter.
**
**	25-FEB-2019	SYS	X.94-03 : Fixed bug with ptlv pointer computation in the __tlv$get(), __tlv$lookup()
**
**	26-FEB-2019	SYS	X.94-04 : 'context' parameter is optional in the __tlv$get()/__tlv$lookup()
**
**	 6-JUN-2019	RRL	X.94-05 : Added zeroing area before copying attribute value in the __tlv$put()
**
**	 2-JUL-2019	RRL	X.94-06 : Parameter 'valsz' of the __tlv$put() is optional for fixed size arguments (WORD, LONGWORD, QUADWORD, UUID)
**
**	29-JUL-2019	RRL	X.94-07 : Redeclared __tlv$dump();
**
**	27-JUL-2026	RRL	X.94-08 : Security/robustness fixes: get/lookup/dump take the real PDU size and validate every TLV against it, 'next TLV' pointer
**				in __tlv$lookup() fixed, <valsz> required for variable-length tags in get, UUID copy clamped, free-space underflow in put fixed.
**
**	28-JUL-2026	RRL	X.94-09 : Routine prefix avproto_ renamed to __tlv$ (uppercase types AVPROTO_PDU/HDR/TLV unchanged).
**
**--
*/

#include	<stdint.h>
#include	<stddef.h>
#include	<ctype.h>
#include	<limits.h>
#include	<stdarg.h>
#include	<string.h>

#include	"utility_routines.h"
#include	"avproto.h"	/* Constants and data structure definitions; <endian.h> or its Windows stand-ins */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Append one TLV record to a PDU.  WORD/LONGWORD/QWORD values are stored in network byte order.  Never
**	writes past <a_pdusz>: the free space is checked against the REAL record length (2/4/8/16 or <a_valsz>
**	for an octet block); the PDU length grows by exactly that plus the four octets of the TLV header.
**
**  FORMAL PARAMETERS:
**
**	a_pdup	Address of the PDU buffer
**	a_pdusz	Size of the PDU buffer in octets
**	a_v_tag	Tag identifier, 0..4095
**	a_v_type	Tag type (TAG$K_*, 0..15)
**	a_val	Address of the value (host byte order for WORD/LONGWORD/QWORD)
**	a_valsz	Size of the value.  Fixed-size type: at least 2/4/8/16, only the type size is stored.
**		Octet block: the record length, 0..65535
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Updates the length field in the PDU header.
**
**  RETURN VALUE:
**	STS$K_SUCCESS; STS$K_ERROR if the buffer is too small, the header length is corrupt, the tag id/type
**	does not fit its bits, or a_valsz is missing/too small.  Nothing is written on error.
**--
*/
int	__tlv$put (
	void *		a_pdup,
	unsigned	a_pdusz,
	unsigned	a_v_tag,
	unsigned	a_v_type,
	void	*	a_val,
	unsigned	a_valsz
		)
{
unsigned	l_len = 0, l_used, l_free, l_fixed = 0;
char	*	l_pchar;
AVPROTO_TLV *	l_ptlv;
AVPROTO_PDU *	l_pdu = (AVPROTO_PDU *) a_pdup;

	/* The PDU area must hold at least the fixed header */
	if ( a_pdusz < sizeof(AVPROTO_HDR) )
		return	$LOG(STS$K_ERROR, "PDU area too small: %u octets", a_pdusz);

	/* The tag id owns twelve bits and the type four: a wider value would corrupt the neighbouring field */
	if ( (a_v_tag > 0x0fff) || (a_v_type > 0xf) )
		return	$LOG(STS$K_ERROR, "Tag id/type out of range: tag=%u, type=%u", a_v_tag, a_v_type);

	/* The real length of the record: the size of the type, or the caller's size for an octet block */
	switch ( a_v_type )
		{
		case	TAG$K_WORD:
			l_len = sizeof(uint16_t), l_fixed = 1;
			break;

		case	TAG$K_LONGWORD:
			l_len = sizeof(uint32_t), l_fixed = 1;
			break;

		case	TAG$K_QWORD:
			l_len = sizeof(uint64_t), l_fixed = 1;
			break;

		case	TAG$K_UUID:
			l_len = 16, l_fixed = 1;
			break;

		default:
			l_len = a_valsz;

			/* The length field of the TLV is sixteen bits wide */
			if ( l_len > USHRT_MAX )
				return	$LOG(STS$K_ERROR, "Value too long for tag=%u: %u octets", a_v_tag, a_valsz);
		}

	/* A fixed-size value must be supplied in full; 0 or a short size is the caller's bug, never guessed */
	if ( l_fixed && (a_valsz < l_len) )
		return	$LOG(STS$K_ERROR, "Value size %u is less than %u required for tag=%u, type=%u", a_valsz, l_len, a_v_tag, a_v_type);

	if ( l_len && !a_val )
		return	$LOG(STS$K_ERROR, "No value for tag=%u, type=%u", a_v_tag, a_v_type);

	/* Get current PDU's payload size from the header */
	l_used = be32toh(l_pdu->r_hdr.u_len);

	/* A declared payload can not be larger than the given PDU area (no unsigned underflow) */
	if ( l_used > (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)) )
		return	$LOG(STS$K_ERROR, "Corrupted PDU: payload=%u > area=%u", l_used, (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)));

	/* Free space left in the PDU's payload area */
	l_free = (unsigned) (a_pdusz - sizeof(AVPROTO_HDR)) - l_used;

	/* Compute pointer to "first free byte" of the PDU's area */
	l_pchar = (char *) l_pdu->r_tlv + l_used;

	/* Check for free space by the real length of the record (l_len <= 65535, no wrap) */
	if ( (sizeof (AVPROTO_TLV) + l_len) > l_free )
		return	$LOG(STS$K_ERROR, "No free space for tag=%u, type=%u, len=%u", a_v_tag, a_v_type, l_len);

	/* Set TLV pointer to end of the current PDU  */
	l_ptlv = (AVPROTO_TLV *) (l_pchar);

	/* Pack Tag type and Tag id into the field */
	l_ptlv->w_tag = __tlv$encode_tag (a_v_type, a_v_tag);
	l_ptlv->w_len = htobe16( (uint16_t) l_len );

	/* Put value into the TLV container; the numeric types cross to network byte order */
	switch ( a_v_type )
		{
		case	TAG$K_WORD:	/* 16 bits */
			{
			uint16_t l_v;				/* unaligned-safe: b_val may sit at an odd offset */

			memcpy(&l_v, a_val, sizeof(l_v));
			l_v = htobe16(l_v);
			memcpy(l_ptlv->b_val, &l_v, sizeof(l_v));
			break;
			}

		case	TAG$K_LONGWORD:	/* 32 bits */
			{
			uint32_t l_v;

			memcpy(&l_v, a_val, sizeof(l_v));
			l_v = htobe32(l_v);
			memcpy(l_ptlv->b_val, &l_v, sizeof(l_v));
			break;
			}

		case	TAG$K_QWORD:	/* 64 bits */
			{
			uint64_t l_v;

			memcpy(&l_v, a_val, sizeof(l_v));
			l_v = htobe64(l_v);
			memcpy(l_ptlv->b_val, &l_v, sizeof(l_v));
			break;
			}

		default:		/* UUID and octet blocks: copied as is */
			if ( l_len )
				memcpy(l_ptlv->b_val, a_val, l_len);
			break;
		}

	/* Adjust PDU's length by exactly the octets written: header + value */
	l_pdu->r_hdr.u_len = htobe32(l_used + sizeof (AVPROTO_TLV) + l_len);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Find a TLV by tag and copy its value to a caller buffer; WORD/LONGWORD/QWORD are converted to host
**	byte order.  The wire length of a fixed-size record must equal the size of its type.
**
**  FORMAL PARAMETERS:
**
**	a_pdup	Address of the received PDU
**	a_pdusz	REAL size of the PDU area
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
**	STS$K_SUCCESS; STS$K_WARN if absent; STS$K_ERROR if the destination is too small, <a_valsz> is
**	missing for a variable-length type, a fixed-size record has a wrong length, or a record runs past the PDU.
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
		/* The fixed TLV header itself must fit into the remaining area; a torn record is an error, not "absent" */
		if ( ((unsigned) l_pos + sizeof(AVPROTO_TLV)) > l_limit )
			{
			l_status = STS$K_ERROR;
			l_pos = l_limit;
			break;
			}

		__tlv$decode_tag (l_ptlv->w_tag, &l_type, &l_tag);
		l_wlen = be16toh(l_ptlv->w_len);

		/* The whole TLV (header + value) must fit too */
		if ( ((unsigned) l_pos + sizeof(AVPROTO_TLV) + l_wlen) > l_limit )
			{
			l_status = STS$K_ERROR;
			l_pos = l_limit;
			break;
			}

		/* Adjust context */
		l_pos += sizeof(AVPROTO_TLV) + l_wlen;

		/* Note: intentional assignment (not ==): l_status becomes 1 on a Tag Id match */
		if ( (l_status = (a_v_tag == l_tag)) )
			break;

		/* Jump to next TLV */
		l_ptlv = (AVPROTO_TLV *) (((char *) l_ptlv) + sizeof(AVPROTO_TLV) + l_wlen);
		}

	if ( a_context )
		*a_context = l_pos;


	/* Not found ? */
	if ( !(1 & l_status) )
		return	l_status;

	/* The wire length of a fixed-size record must match its type: a hostile record must not be over-read or half-copied */
	if ( ((l_type == TAG$K_WORD) && (l_wlen != sizeof(uint16_t))) || ((l_type == TAG$K_LONGWORD) && (l_wlen != sizeof(uint32_t)))
		|| ((l_type == TAG$K_QWORD) && (l_wlen != sizeof(uint64_t))) || ((l_type == TAG$K_UUID) && (l_wlen != 16)) )
		return	$LOG(STS$K_ERROR, "Bad length %u of the record tag=%u, type=%u", l_wlen, l_tag, l_type);

	if ( !a_val || !a_v_type )
		return	STS$K_ERROR;

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
			/* l_wlen == 16 is proven above and *a_valsz >= l_wlen by the destination check */
			memcpy(a_val, l_ptlv->b_val, 16);
			l_len = 16;
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
**	Find a TLV by tag and return a pointer to its value inside the PDU, no copy, NOT byte-order converted;
**	valid only while the caller's PDU buffer lives.
**
**  FORMAL PARAMETERS:
**
**	a_pdup	Address of the received PDU
**	a_pdusz	REAL size of the PDU area
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
**	STS$K_SUCCESS; STS$K_WARN if absent; STS$K_ERROR if a record runs past the PDU or a fixed-size record
**	(WORD 2, LONGWORD 4, QWORD 8, UUID 16) has a wrong wire length.
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
		/* The fixed TLV header itself must fit into the remaining area; a torn record is an error, not "absent" */
		if ( ((unsigned) l_pos + sizeof(AVPROTO_TLV)) > l_limit )
			{
			l_status = STS$K_ERROR;
			l_pos = l_limit;
			break;
			}

		__tlv$decode_tag (l_ptlv->w_tag, &l_type, &l_tag);
		l_wlen = be16toh(l_ptlv->w_len);

		/* The whole TLV (header + value) must fit too */
		if ( ((unsigned) l_pos + sizeof(AVPROTO_TLV) + l_wlen) > l_limit )
			{
			l_status = STS$K_ERROR;
			l_pos = l_limit;
			break;
			}

		/* Adjust context */
		l_pos += sizeof(AVPROTO_TLV) + l_wlen;

		/* Note: intentional assignment (not ==): l_status becomes 1 on a Tag Id match */
		if ( (l_status = (a_v_tag == l_tag)) )
			break;

		/* Jump to next TLV - relative to the CURRENT TLV, not to the PDU base ! */
		l_ptlv = (AVPROTO_TLV *) (((char *) l_ptlv) + sizeof(AVPROTO_TLV) + l_wlen);
		}

	if ( a_context )
		*a_context = l_pos;

	/* Not found ? */
	if ( !(1 & l_status) )
		return	l_status;

	/* The wire length of a fixed-size record must match its type, as in __tlv$get(): never a pointer to a short record */
	if ( ((l_type == TAG$K_WORD) && (l_wlen != sizeof(uint16_t))) || ((l_type == TAG$K_LONGWORD) && (l_wlen != sizeof(uint32_t)))
		|| ((l_type == TAG$K_QWORD) && (l_wlen != sizeof(uint64_t))) || ((l_type == TAG$K_UUID) && (l_wlen != 16)) )
		return	$LOG(STS$K_ERROR, "Bad length %u of the record tag=%u, type=%u", l_wlen, l_tag, l_type);

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
**	Write every TLV of a PDU to the log: tag, type, length and a hex dump of the value (continued in
**	64-octet lines).  For debugging.
**
**  FORMAL PARAMETERS:
**
**	a_pdu	Address of the PDU
**	a_pdusz	REAL size of the PDU area
**--
*/
void	__tlv$dump	(
		void	*a_pdu,
		unsigned a_pdusz		/* Real size of the PDU area (received octets) */
			)
{
char		l_hexbuf1[2 * sizeof(AVPROTO_TLV) + 1], l_hexbuf2[2 * 64 + 1];
char	*	l_pchar;
unsigned	l_limit, l_count, l_pos, l_off, l_chunk;
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
		/* The value goes out in chunks of 64 octets, so a long one can not overflow the hex buffer */
		l_off = 0;
		do	{
			l_chunk = $MIN(l_wlen - l_off, (sizeof(l_hexbuf2) - 1) / 2);

			__util$bin2hex (l_ptlv->b_val + l_off, l_hexbuf2, l_chunk);
			l_hexbuf2[2 * l_chunk] = '\0';

			if ( !l_off )
				$LOG(STS$K_INFO, "[%04.4d] TLV [tag=%04x(id=%04x, type=%04x), len=%02d] 0x%s:0x%s",
				     l_count, l_wtag, l_vtag, l_vtype, l_wlen, l_hexbuf1, l_hexbuf2);
			else
				$LOG(STS$K_INFO, "[%04.4d]   +%04u: 0x%s", l_count, l_off, l_hexbuf2);

			l_off += l_chunk;
			} while ( l_off < l_wlen );

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


/* __CLIENT - demo TLS client: connect and exchange a request/response PDU pair (__AVPROTO_MAIN__ harness, not library API) */
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

/* __SERVER - demo TLS server: accept a connection, read a request PDU, answer with a response PDU (__AVPROTO_MAIN__ harness, not library API) */
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



/* PDU_RECV - read exactly one PDU from a TLS socket: the fixed header, then the payload its length declares (__AVPROTO_MAIN__ harness, not library API) */
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


/* PDU_XMIT - write one complete PDU (header plus payload) to a TLS socket (__AVPROTO_MAIN__ harness, not library API) */
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


/* INIT_SSL - create and configure the demo OpenSSL context (method, certificate, key, verification mode) (__AVPROTO_MAIN__ harness, not library API) */
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

/* KDEPO_GET_KEY - fetch the drive keys from a KDEPO server: send the computer UUID, receive a key block per drive (__AVPROTO_MAIN__ harness, not library API) */
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



/* MAIN - demo entry point: get the drive keys from the KDEPO server and print them (__AVPROTO_MAIN__ harness, not library API) */
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
		/* Print the disk keys received (demo only) */
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
