#ifndef	__AVPROTO$DEF__
#define	__AVPROTO$DEF__	1


#ifdef _WIN32
	#pragma once
	#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif


/*
**++
**
**  FACILITY:  AVProto - Attribute/Value Pair (AKA TLV) based protocol
**
**  ABSTRACT: Carrying data encapsulated in to the AVP microcontainers
**
**  DESCRIPTION: This module contains data structures and constant definition is supposed to be used
**	in the core AVProto routines as well as in general API routines.
**
**  DESIGN ISSUE:
**
**	A main idea is imagined follows :
**
	PDU (protocol/packet data unit ) - is a block is supposed to be send to remote peer over network

		 |<- Payload (see Header.len field)-->|
	+--------+-------+-------+--    ...  ----+-------+
	| Header | TLV 0 | TLV 1 |          .... | TLV N |
	+--------+-------+-------+----  ...    --+-------+

	PDU's Header :
	+-----------+-----------+-----------+-----------+
	| magic     |  len      |  csr      |   seq     |
	| 8 octets  |  4 octets |  4 octets |   4 octets
	+-----------+-----------+-----------+-----------+

	T[ag] L[ength] V[alue] :
	0                    16                  32
	+---------+-----------+-------------------+
	| Type    |   Id      |      Length       |
	| 4 bits  |   12 bits |      16 bits      |
	+---------+-----------+-------------------+
	|    optional data (value)                |
	|      0 - 'Length' octets                |
	+---------+-----------+-------------------+
**
**  AUTHORS: Ruslan R. Laishev (RRL)
**
**  CREATION DATE:  20-JUL-2018
**
**  MODIFICATION HISTORY:
**
**	15-FEB-2019	RRL	Removed bitfields.
**
**	22-FEB-2019	RRL	Added closing for "#ifdef __cplusplus"
**				some code reorganizing;
**
**	29-JUL-2019	RRL	Added declaration for avproto_dump();
**				improved error handling in the avproto_hget();
**
**
**--
*/

#include	<string.h>
#include	<endian.h>


#ifdef __cplusplus
extern "C" {
#endif



/*
 * AVP/TLV - type of block types to help transparent conversion of the well-know datas
 * is carried in NBO or to help LE/Be conversion
 */
enum	{
	TAG$K_BBLOCK = 0,	/* A default type - octets block	*/
	TAG$K_WORD = 3,		/* 16-bits word, NBO			*/
	TAG$K_LONGWORD,		/* 32-bits word, NBO			*/
	TAG$K_QWORD,		/* 64-bits word, NBO			*/
	TAG$K_UUID = 8,		/* UUID					*/


	TAG$K_MAX		/* EOL marker, must be last!		*/
};


#pragma	pack (push, 1)

#define	TAG$M_TYPE	0xf000
#define	TAG$M_ID	(~(TAG$M_TYPE))

typedef struct __avproto_tlv__ {

	unsigned short	w_tag;	/* Tag's type and id			*/
	unsigned short	w_len;	/* Actual data length in the b_val[]	*/

	union	{		/* Placeholder for data block		*/
		unsigned char	b_val[0];
		unsigned short	w_val[0];
		unsigned	u_val[0];
	unsigned long long	q_val[0];
	};
} AVPROTO_TLV;

typedef struct __avproto_hdr__ {
	unsigned char	magic[8];
	unsigned	u_len;		/* A length of payload follows header	*/
	unsigned	u_csr;		/* Command/Status Register		*/

	unsigned	u_seq;		/* A PDU sequence number is supposed to help
					  parallel and asyncronus processing	*/

} AVPROTO_HDR;

typedef struct __avproto_pdu__ {
	AVPROTO_HDR	r_hdr;		/* Fixed length header			*/
	AVPROTO_TLV	r_tlv[0];	/* Placeholder for variable part of PDU	*/
} AVPROTO_PDU;

#pragma	pack (pop)

/*
 *  DESCRIPTION: avproto_encode_tag - encapsulate given 'v_type' and  'v_tag' into the 16-bits field.
 *	return NBO representation of the w_tag;
 *
 *  INPUTS:
 *	v_type:	Tag type, see TAG$K_* constants
 *	v_tag:	Tag Id, application specific Tag Id
 *
 *  OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	16 bits TLV.w_tag field, NBO
 */
inline	static unsigned short avproto_encode_tag
		(
	unsigned short	a_v_type,
	unsigned short	a_v_tag
		)
{
	/* Pack layout: w_tag = [ type : 4 bits | id : 12 bits ], returned in NBO */
	return	htobe16 ( ( (a_v_type << 12) | a_v_tag ));
}

/*
 *  DESCRIPTION: avproto_decode_tag - extract 'tag id' and 'tag type' field from the TLV.w_tag.
 *	Return w_tag in the Host Order Byte
 *
 *  INPUTS:
 *	w_tag:	TLV.w_tag field, NBO
 *
 *  OUTPUTS:
 *	v_type:	an address of variable to accept unpacked 'tag type' field
 *	v_tag:	an address of variable to accept unpacked 'tag id field
 *
 *   RETURNS:
 *	16 bits, TLV_w_tag in Host Order Byte
 */
inline static unsigned short	avproto_decode_tag
		(
	unsigned short	a_w_tag,
	unsigned short	*a_v_type,
	unsigned short	*a_v_tag
		)
{
unsigned short	l_tag = 0;

	l_tag = be16toh(a_w_tag);

	/* Unpack: high 4 bits -> type, low 12 bits -> id (see TAG$M_TYPE/TAG$M_ID) */
	*a_v_type = l_tag >> 12;
	*a_v_tag = l_tag & TAG$M_ID;

	return	l_tag;
}


int	avproto_put    (void *a_pdu, unsigned short a_pdusz, unsigned a_v_tag, unsigned a_v_type, void *a_val, unsigned a_valsz);
int	avproto_get    (void *a_pdu, unsigned a_pdusz, int *a_context, unsigned a_v_tag, unsigned *a_v_type, void *a_val, unsigned *a_valsz);
int	avproto_lookup (void *a_pdu, unsigned a_pdusz, int *a_context, unsigned a_v_tag, unsigned *a_v_type, void **a_val, unsigned *a_valsz);
void	avproto_dump   (void *a_pdu, unsigned a_pdusz);


/*
 *  DESCRIPTION: Initialize given PDU's header with given values, reset length of the PDU
 *	to initial.
 *
 *  INPUTS:
 *	sig:	A signature string, ASCIZ
 *	pdu:	A Protocol Data Unit
 *	u_csr:	A command/status register field
 *	u_seq:	PDU's sequence field
 *
 *  OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	condition code
 */
inline static int	avproto_hset	(
		void		*a_pdup,
	const unsigned char	*a_sig,
		unsigned	a_u_csr,
		unsigned	a_u_seq
			)
{
size_t		l_siglen;
AVPROTO_PDU	*l_pdu = (AVPROTO_PDU *) a_pdup;

	l_siglen = strnlen((const char *) a_sig, sizeof(l_pdu->r_hdr.magic));

	memset(&l_pdu->r_hdr, 0, sizeof(AVPROTO_HDR));
	memcpy(l_pdu->r_hdr.magic, a_sig, l_siglen);		/* rest stays zero-padded by memset above */

	l_pdu->r_hdr.u_csr = htobe32(a_u_csr);
	l_pdu->r_hdr.u_seq = htobe32(a_u_seq);

	return	STS$K_SUCCESS;
}


/*
 *  DESCRIPTION: Get information from the PDU's header.
 *
 *  INPUTS:
 *	pdu:	A Protocol Data Unit
 *	sig:	A signature string, ASCIZ
 *
 *  OUTPUTS:
 *	u_len:	A length of the PDU's payload
 *	u_csr:	A command/status register field
 *	u_seq:	PDU's sequence field
 *
 *   RETURNS:
 *	condition code
 */
inline static int	avproto_hget	(
		void		*a_pdup,
	const unsigned char	*a_sig,
		unsigned	*a_u_len,
		unsigned	*a_u_csr,
		unsigned	*a_u_seq
			)
{
size_t		l_len;
AVPROTO_PDU	*l_pdu = (AVPROTO_PDU *) a_pdup;

	*a_u_csr = STS$K_ERROR;

	if ( (l_len = strnlen((const char *) a_sig, sizeof(l_pdu->r_hdr.magic) + 1)) > sizeof(l_pdu->r_hdr.magic) )
		return	STS$K_ERROR;

	if ( memcmp(l_pdu->r_hdr.magic, a_sig, sizeof (l_pdu->r_hdr.magic)) )
		return	STS$K_ERROR;

	*a_u_len = be32toh(l_pdu->r_hdr.u_len);
	*a_u_csr = be32toh(l_pdu->r_hdr.u_csr);
	*a_u_seq = be32toh(l_pdu->r_hdr.u_seq);

	return	STS$K_SUCCESS;
}


#ifdef __cplusplus
	}
#endif

#endif	/* #ifndef	__AVPROTO$DEF__	*/
