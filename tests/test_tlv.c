/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  Round-trips PDUs through the avproto (__tlv$) Tag-Length-Value
**	API: builds a PDU with __tlv$hset()/__tlv$put(), then reads it back with
**	__tlv$hget()/__tlv$get()/__tlv$lookup() and checks that every field and
**	value - including the network/host byte-order conversion of the word,
**	longword and quadword types - survives the trip.
**
**  MODULE:  test_tlv.c
**--
*/
#define __MODULE__ "TLVT$"

#include <string.h>
#include <stdint.h>
#include "test_harness.h"
#include "utility_routines.h"
#include "avproto.h"

#define	MY_TAG_BLOB	100

int main (void)
{
	unsigned char		l_pdu[512];
	/* Deliberately shorter than the 8-byte magic field: exercises the zero-
	** padding in __tlv$hget() (a short signature must not read past its end).
	*/
	const unsigned char	*l_sig   = (const unsigned char *) "TSTSIG";
	const unsigned char	*l_wrong = (const unsigned char *) "OTHER";

	uint16_t		l_w = 0xABCD;
	uint32_t		l_l = 0xDEADBEEF;
	uint64_t		l_q = 0x1122334455667788ULL;
	unsigned char		l_blob[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	unsigned		l_hlen, l_hcsr, l_hseq;
	int			l_ctx;
	unsigned		l_vt;

	printf("== tlv round-trip regression ==\n");

	/* Build the PDU. */
	CHECK( 1 & __tlv$hset(l_pdu, l_sig, 0x1234, 42), "hset must succeed");

	CHECK( 1 & __tlv$put(l_pdu, sizeof(l_pdu), TAG$K_WORD,     TAG$K_WORD,     &l_w,  sizeof(l_w)),  "put WORD");
	CHECK( 1 & __tlv$put(l_pdu, sizeof(l_pdu), TAG$K_LONGWORD, TAG$K_LONGWORD, &l_l,  sizeof(l_l)),  "put LONGWORD");
	CHECK( 1 & __tlv$put(l_pdu, sizeof(l_pdu), TAG$K_QWORD,    TAG$K_QWORD,    &l_q,  sizeof(l_q)),  "put QWORD");
	CHECK( 1 & __tlv$put(l_pdu, sizeof(l_pdu), MY_TAG_BLOB,    TAG$K_BBLOCK,   l_blob, sizeof(l_blob)), "put BBLOCK");

	/* Header round-trip. */
	CHECK( 1 & __tlv$hget(l_pdu, l_sig, &l_hlen, &l_hcsr, &l_hseq), "hget must succeed");
	CHECK( l_hcsr == 0x1234, "hget must round-trip csr (0x1234, got %#x)", l_hcsr);
	CHECK( l_hseq == 42,     "hget must round-trip seq (42, got %u)", l_hseq);
	CHECK( !(1 & __tlv$hget(l_pdu, l_wrong, &l_hlen, &l_hcsr, &l_hseq)),
		"hget with a wrong signature must fail");

	/* Value round-trips - the numeric types cross the byte order and come back. */
	{
	uint16_t l_w2 = 0; l_ctx = -1;
	CHECK( (1 & __tlv$get(l_pdu, sizeof(l_pdu), &l_ctx, TAG$K_WORD, &l_vt, &l_w2, NULL)) && (l_w2 == l_w),
		"WORD must round-trip (%#x, got %#x)", l_w, l_w2);
	}
	{
	uint32_t l_l2 = 0; l_ctx = -1;
	CHECK( (1 & __tlv$get(l_pdu, sizeof(l_pdu), &l_ctx, TAG$K_LONGWORD, &l_vt, &l_l2, NULL)) && (l_l2 == l_l),
		"LONGWORD must round-trip (%#x, got %#x)", l_l, l_l2);
	}
	{
	uint64_t l_q2 = 0; l_ctx = -1;
	CHECK( (1 & __tlv$get(l_pdu, sizeof(l_pdu), &l_ctx, TAG$K_QWORD, &l_vt, &l_q2, NULL)) && (l_q2 == l_q),
		"QWORD must round-trip");
	}
	{
	unsigned char l_b2[16]; unsigned l_bl = sizeof(l_b2); l_ctx = -1;
	CHECK( (1 & __tlv$get(l_pdu, sizeof(l_pdu), &l_ctx, MY_TAG_BLOB, &l_vt, l_b2, &l_bl))
			&& (l_bl == sizeof(l_blob)) && (0 == memcmp(l_b2, l_blob, sizeof(l_blob))),
		"BBLOCK must round-trip");
	}

	/* A tag that was never put must not be found. */
	{
	uint16_t l_w2 = 0; l_ctx = -1;
	CHECK( !(1 & __tlv$get(l_pdu, sizeof(l_pdu), &l_ctx, 999, &l_vt, &l_w2, NULL)),
		"a missing tag must not be reported as found");
	}

	/* __tlv$lookup returns a pointer into the PDU without a copy. */
	{
	void *l_pv = NULL; unsigned l_pl = 0; l_ctx = -1;
	CHECK( (1 & __tlv$lookup(l_pdu, sizeof(l_pdu), &l_ctx, MY_TAG_BLOB, &l_vt, &l_pv, &l_pl))
			&& (l_pl == sizeof(l_blob)) && l_pv && (0 == memcmp(l_pv, l_blob, sizeof(l_blob))),
		"lookup must point at the BBLOCK value");
	}

	TEST_DONE();
}
