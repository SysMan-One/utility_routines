/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  Hostile and malformed input for the avproto (__tlv$) API:
**	overrun attempts of __tlv$put() for every type, truncated and
**	malformed records for __tlv$get()/__tlv$lookup(), a short UUID, a dump
**	of a 300-octet value, a signature longer than eight octets, and the
**	agreement of the PDU header length with the records. Every buffer is an
**	exact-size heap block, so under AddressSanitizer any overrun is fatal.
**
**  MODULE:  test_tlv_hostile.c
**--
*/
#define __MODULE__ "TLVH$"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "test_harness.h"
#include "utility_routines.h"
#include "avproto.h"

static unsigned char *s_mk (size_t a_sz, const char *a_sig)
{
unsigned char	*l_p = malloc(a_sz);

	memset(l_p, 0xA5, a_sz);
	__tlv$hset(l_p, (const unsigned char *) a_sig, 1, 2);
	return	l_p;
}

static unsigned s_len (unsigned char *a_p)
{
unsigned	l_len = 0, l_csr, l_seq;

	__tlv$hget(a_p, (const unsigned char *) "SIG", &l_len, &l_csr, &l_seq);
	return	l_len;
}

int main (void)
{
unsigned char	*l_pdu, l_uuid[16], l_big[300], l_out[512];
uint64_t	l_q = 0x1122334455667788ULL;
unsigned	l_vt, l_sz, l_hl, l_hc, l_hs;
int		l_ctx, l_fd;
char		l_log[8192];
ssize_t		l_n;

	printf("== tlv hostile input ==\n");

	for (int i = 0; i < 16; i++)
		l_uuid[i] = i + 1;
	memset(l_big, 0x5A, sizeof(l_big));

	l_fd = open("/dev/null", O_WRONLY);		/* the library logs every refusal: keep the output clean */
	__util$setlogfd(l_fd);

	/* --- put: overrun attempts, one per type. The area holds the header plus exactly one octet short of the record --- */
	{
	static const struct { unsigned type, rec; } l_t[] = { { TAG$K_WORD, 2 }, { TAG$K_LONGWORD, 4 }, { TAG$K_QWORD, 8 }, { TAG$K_UUID, 16 }, { TAG$K_BBLOCK, 10 } };

	for (int i = 0; i < 5; i++)
		{
		unsigned	l_sz = sizeof(AVPROTO_HDR) + 4 + l_t[i].rec - 1;
		unsigned char	*l_p = s_mk(l_sz, "SIG");

		CHECK( !(1 & __tlv$put(l_p, l_sz, 1, l_t[i].type, l_uuid, l_t[i].rec)), "put type %u into an area one octet short must fail", l_t[i].type);
		CHECK( 0 == s_len(l_p), "failed put must not change the PDU length (type %u)", l_t[i].type);
		free(l_p);

		l_sz++;
		l_p = s_mk(l_sz, "SIG");
		CHECK( 1 & __tlv$put(l_p, l_sz, 1, l_t[i].type, l_uuid, l_t[i].rec), "put type %u into an exact area must succeed", l_t[i].type);
		CHECK( s_len(l_p) == 4 + l_t[i].rec, "PDU length must be 4+%u, got %u", l_t[i].rec, s_len(l_p));
		free(l_p);
		}
	}

	/* --- put: valsz 0 / too small for a fixed type is an error, never silently the type size --- */
	l_pdu = s_mk(sizeof(AVPROTO_HDR) + 64, "SIG");
	CHECK( !(1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 1, TAG$K_QWORD, &l_q, 0)), "QWORD valsz 0 must fail");
	CHECK( !(1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 1, TAG$K_QWORD, &l_q, 4)), "QWORD valsz 4 must fail");
	CHECK( !(1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 1, TAG$K_UUID, l_uuid, 4)), "UUID valsz 4 must fail");
	CHECK( !(1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 1, TAG$K_WORD, &l_q, 1)), "WORD valsz 1 must fail");
	CHECK( !(1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 1, TAG$K_LONGWORD, NULL, 4)), "NULL value must fail");
	CHECK( 0 == s_len(l_pdu), "refused puts leave the PDU empty");

	/* --- put: tag id 12 bits, type 4 bits --- */
	CHECK( !(1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 0x1000, TAG$K_QWORD, &l_q, 8)), "tag id 0x1000 must be rejected");
	CHECK( !(1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 1, 16, &l_q, 8)), "type 16 must be rejected");
	CHECK( 1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 0xfff, TAG$K_QWORD, &l_q, 8), "tag id 0xfff fits");
	CHECK( 1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 64, 0xabc, TAG$K_BBLOCK, l_uuid, 0), "empty block with id 0xabc fits");
	{
	unsigned l_ty = 0xff; unsigned short l_t2, l_g2;
	l_ctx = -1;
	CHECK( (1 & __tlv$get(l_pdu, sizeof(AVPROTO_HDR) + 64, &l_ctx, 0xfff, &l_vt, &l_q, NULL)) && (l_vt == TAG$K_QWORD), "id 0xfff reads back as QWORD");
	__tlv$decode_tag(((AVPROTO_PDU *) l_pdu)->r_tlv[0].w_tag, &l_t2, &l_g2);
	CHECK( (l_t2 == TAG$K_QWORD) && (l_g2 == 0xfff) && (l_ty == 0xff), "type/id fields did not bleed into each other");
	}
	free(l_pdu);

	/* --- put: a block above 65535 octets and an area larger than 64 KB (buffer size is unsigned) --- */
	{
	unsigned	l_sz = sizeof(AVPROTO_HDR) + 100000;
	unsigned char	*l_p = s_mk(l_sz, "SIG"), *l_blob = calloc(1, 70000);

	CHECK( !(1 & __tlv$put(l_p, l_sz, 1, TAG$K_BBLOCK, l_blob, 65536)), "a 65536-octet block does not fit the 16-bit length");
	CHECK( 1 & __tlv$put(l_p, l_sz, 1, TAG$K_BBLOCK, l_blob, 65535), "a 65535-octet block fits a 100000-octet area");
	CHECK( s_len(l_p) == 65539, "PDU length 65539, got %u", s_len(l_p));
	free(l_p);
	free(l_blob);
	}

	/* --- put: header len lies --- */
	l_pdu = s_mk(sizeof(AVPROTO_HDR) + 16, "SIG");
	((AVPROTO_PDU *) l_pdu)->r_hdr.u_len = htobe32(1000);
	CHECK( !(1 & __tlv$put(l_pdu, sizeof(AVPROTO_HDR) + 16, 1, TAG$K_WORD, &l_q, 2)), "put into a PDU whose u_len exceeds the area must fail");
	CHECK( !(1 & __tlv$put(l_pdu, 3, 1, TAG$K_WORD, &l_q, 2)), "put into an area smaller than the header must fail");
	free(l_pdu);

	/* --- header length and record length agree for every type, including the UUID (used to add 4+valsz) --- */
	l_pdu = s_mk(512, "SIG");
	CHECK( 1 & __tlv$put(l_pdu, 512, 1, TAG$K_UUID, l_uuid, 16), "put UUID");
	CHECK( s_len(l_pdu) == 20, "UUID record: PDU length 20, got %u", s_len(l_pdu));
	CHECK( 1 & __tlv$put(l_pdu, 512, 2, TAG$K_UUID, l_big, 40), "put UUID with a larger valsz stores 16");
	CHECK( s_len(l_pdu) == 40, "second UUID record: PDU length 40, got %u", s_len(l_pdu));
	CHECK( 1 & __tlv$put(l_pdu, 512, 3, TAG$K_WORD, &l_q, 8), "put WORD with a larger valsz stores 2");
	CHECK( s_len(l_pdu) == 46, "WORD record: PDU length 46, got %u", s_len(l_pdu));
	CHECK( be16toh(((AVPROTO_TLV *) ((char *) ((AVPROTO_PDU *) l_pdu)->r_tlv + 40))->w_len) == 2, "WORD wire length is 2");
	l_ctx = -1; l_sz = 16;
	CHECK( (1 & __tlv$get(l_pdu, 512, &l_ctx, 1, &l_vt, l_out, &l_sz)) && (l_sz == 16) && !memcmp(l_out, l_uuid, 16), "UUID round-trips");
	free(l_pdu);

	/* --- get/lookup: malformed records, one per type --- */
	{
	static const struct { unsigned type, wire; } l_m[] = { { TAG$K_WORD, 3 }, { TAG$K_WORD, 1 }, { TAG$K_LONGWORD, 2 }, { TAG$K_LONGWORD, 8 },
		{ TAG$K_QWORD, 4 }, { TAG$K_QWORD, 0 }, { TAG$K_UUID, 4 }, { TAG$K_UUID, 0 }, { TAG$K_UUID, 17 } };

	for (int i = 0; i < 9; i++)
		{
		unsigned	l_sz = sizeof(AVPROTO_HDR) + 4 + l_m[i].wire;
		unsigned char	*l_p = s_mk(l_sz, "SIG");
		AVPROTO_PDU	*l_x = (AVPROTO_PDU *) l_p;

		l_x->r_hdr.u_len = htobe32(4 + l_m[i].wire);
		l_x->r_tlv[0].w_tag = __tlv$encode_tag(l_m[i].type, 7);
		l_x->r_tlv[0].w_len = htobe16(l_m[i].wire);
		memset(l_x->r_tlv[0].b_val, 0x11, l_m[i].wire);

		l_ctx = -1; l_sz = sizeof(l_out);
		CHECK( !(1 & __tlv$get(l_p, sizeof(AVPROTO_HDR) + 4 + l_m[i].wire, &l_ctx, 7, &l_vt, l_out, &l_sz)),
			"get of type %u with wire length %u must fail", l_m[i].type, l_m[i].wire);
		{
		void	*l_pv = NULL;

		l_ctx = -1; l_sz = 0;
		CHECK( !(1 & __tlv$lookup(l_p, sizeof(AVPROTO_HDR) + 4 + l_m[i].wire, &l_ctx, 7, &l_vt, &l_pv, &l_sz)) && !l_pv,
			"lookup of type %u with wire length %u must fail, no pointer", l_m[i].type, l_m[i].wire);
		}
		free(l_p);
		}
	}

	/* --- get/lookup: records running past the PDU (area exactly as declared; length field claims more) --- */
	{
	static const unsigned l_ty[] = { TAG$K_WORD, TAG$K_LONGWORD, TAG$K_QWORD, TAG$K_UUID, TAG$K_BBLOCK };

	for (int i = 0; i < 5; i++)
		{
		unsigned	l_sz = sizeof(AVPROTO_HDR) + 4 + 2;
		unsigned char	*l_p = s_mk(l_sz, "SIG");
		AVPROTO_PDU	*l_x = (AVPROTO_PDU *) l_p;
		void		*l_pv;

		l_x->r_hdr.u_len = htobe32(4 + 2);
		l_x->r_tlv[0].w_tag = __tlv$encode_tag(l_ty[i], 7);
		l_x->r_tlv[0].w_len = htobe16(60000);

		l_ctx = -1; l_sz = sizeof(l_out);
		CHECK( !(1 & __tlv$get(l_p, sizeof(AVPROTO_HDR) + 6, &l_ctx, 7, &l_vt, l_out, &l_sz)),
			"get of an overlong type %u record must fail", l_ty[i]);
		l_ctx = -1; l_sz = 0;
		CHECK( !(1 & __tlv$lookup(l_p, sizeof(AVPROTO_HDR) + 6, &l_ctx, 7, &l_vt, &l_pv, &l_sz)), "lookup of an overlong type %u record must fail", l_ty[i]);
		free(l_p);
		}
	}

	/* a torn TLV header (1..3 octets of payload) is an error too; an empty payload is simply "absent" */
	for (unsigned i = 0; i < 4; i++)
		{
		unsigned	l_sz = sizeof(AVPROTO_HDR) + i;
		unsigned char	*l_p = s_mk(l_sz, "SIG");

		((AVPROTO_PDU *) l_p)->r_hdr.u_len = htobe32(i);
		l_ctx = -1; l_sz = sizeof(l_out);
		l_hl = __tlv$get(l_p, sizeof(AVPROTO_HDR) + i, &l_ctx, 7, &l_vt, l_out, &l_sz);
		CHECK( i ? !(1 & l_hl) : (l_hl == STS$K_WARN), "payload of %u octets: status %d", i, l_hl);
		free(l_p);
		}

	/* a lying header (u_len larger than the area) is clamped to the area */
	l_pdu = s_mk(sizeof(AVPROTO_HDR) + 8, "SIG");
	((AVPROTO_PDU *) l_pdu)->r_hdr.u_len = htobe32(0x7fffffff);
	l_ctx = -1; l_sz = sizeof(l_out);
	CHECK( !(1 & __tlv$get(l_pdu, sizeof(AVPROTO_HDR) + 8, &l_ctx, 7, &l_vt, l_out, &l_sz)), "a lying u_len over junk gives an error status, not a crash");
	free(l_pdu);

	/* --- the short UUID (the old movc5 NULL-fill crash) --- */
	{
	unsigned	l_sz = sizeof(AVPROTO_HDR) + 4 + 5;
	unsigned char	*l_p = s_mk(l_sz, "SIG");
	AVPROTO_PDU	*l_x = (AVPROTO_PDU *) l_p;

	l_x->r_hdr.u_len = htobe32(4 + 5);
	l_x->r_tlv[0].w_tag = __tlv$encode_tag(TAG$K_UUID, 9);
	l_x->r_tlv[0].w_len = htobe16(5);
	memcpy(l_x->r_tlv[0].b_val, l_uuid, 5);

	memset(l_out, 0xEE, sizeof(l_out));
	l_ctx = -1; l_sz = 16;
	CHECK( !(1 & __tlv$get(l_p, sizeof(AVPROTO_HDR) + 9, &l_ctx, 9, &l_vt, l_out, &l_sz)), "a 5-octet UUID record must be an error");
	CHECK( l_out[0] == 0xEE, "a refused UUID must not be copied");
	free(l_p);
	}

	/* an UUID read into a destination smaller than 16 is refused */
	l_pdu = s_mk(512, "SIG");
	__tlv$put(l_pdu, 512, 9, TAG$K_UUID, l_uuid, 16);
	l_ctx = -1; l_sz = 8;
	CHECK( !(1 & __tlv$get(l_pdu, 512, &l_ctx, 9, &l_vt, l_out, &l_sz)), "UUID into an 8-octet destination must fail");

	/* --- dump of a 300-octet value: no overflow, every chunk shown --- */
	__tlv$put(l_pdu, 512, 10, TAG$K_BBLOCK, l_big, 300);
	{
	char	l_name[] = "/tmp/tlvh-XXXXXX";
	int	l_lf = mkstemp(l_name);

	__util$setlogfd(l_lf);
	__tlv$dump(l_pdu, 512);
	__util$setlogfd(l_fd);

	lseek(l_lf, 0, SEEK_SET);
	l_n = read(l_lf, l_log, sizeof(l_log) - 1);
	l_log[l_n > 0 ? l_n : 0] = '\0';
	close(l_lf);
	unlink(l_name);

	CHECK( strstr(l_log, "len=300"), "dump shows the 300-octet record");
	CHECK( strstr(l_log, "+0256"), "dump continues up to offset 256");
	CHECK( strstr(l_log, "5a5a5a5a") || strstr(l_log, "5A5A5A5A"), "dump shows the value octets");
	}
	free(l_pdu);

	/* --- signature longer than 8 octets: rejected by both hset and hget; csr untouched on error --- */
	{
	unsigned char	l_h[64];
	const unsigned char *l_long = (const unsigned char *) "ABCDEFGHI";

	memset(l_h, 0x77, sizeof(l_h));
	CHECK( !(1 & __tlv$hset(l_h, l_long, 1, 2)), "hset with a 9-octet signature must fail");
	CHECK( l_h[0] == 0x77, "failed hset must not touch the PDU");
	CHECK( 1 & __tlv$hset(l_h, (const unsigned char *) "ABCDEFGH", 5, 6), "hset with an 8-octet signature is fine");
	CHECK( 1 & __tlv$hget(l_h, (const unsigned char *) "ABCDEFGH", &l_hl, &l_hc, &l_hs), "hget with the 8-octet signature");

	l_hl = l_hc = l_hs = 0xDEAD;
	CHECK( !(1 & __tlv$hget(l_h, l_long, &l_hl, &l_hc, &l_hs)), "hget with a 9-octet signature must fail");
	CHECK( (l_hc == 0xDEAD) && (l_hl == 0xDEAD) && (l_hs == 0xDEAD), "failed hget (long signature) must not write its outputs");
	l_hl = l_hc = l_hs = 0xDEAD;
	CHECK( !(1 & __tlv$hget(l_h, (const unsigned char *) "WRONG", &l_hl, &l_hc, &l_hs)), "hget with a wrong signature must fail");
	CHECK( (l_hc == 0xDEAD) && (l_hl == 0xDEAD) && (l_hs == 0xDEAD), "failed hget (wrong signature) must not write its outputs");
	}

	TEST_DONE();
}
