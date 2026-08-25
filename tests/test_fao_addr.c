/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  The FAO address directives !%M (MAC/hardware address), !%I (IPv4),
**	!%6 (IPv6) and !%U (UUID). Each takes a pointer to the raw octets and emits
**	the customary text; a NULL pointer yields "(null)" rather than a fault.
**
**  MODULE:  test_fao_addr.c
**--
*/
#define __MODULE__ "FAOA$"

#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "test_harness.h"
#include "starlet.h"

int main (void)
{
char		l_out [256];
unsigned short	l_len = 0;
fao_desc_t	l_d;
fao_prm_t	l_p [2];
unsigned char	l_mac [6]  = { 0x00,0x1b,0x21,0x3c,0x4d,0x5e };
unsigned char	l_ip4 [4]  = { 192,168,1,10 };
unsigned char	l_ip6 [16] = { 0x20,0x01,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,1 };
unsigned char	l_uu  [16] = { 0x55,0x0e,0x84,0x00,0xe2,0x9b,0x41,0xd4,
					0xa7,0x16,0x44,0x66,0x55,0x44,0x00,0x00 };

	printf("== fao address directives ==\n");

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_mac;
	__util$faol ("!%M", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "00:1b:21:3c:4d:5e"), "the MAC directive must format a MAC: '%s'", l_out);

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_ip4;
	__util$faol ("!%I", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "192.168.1.10"), "the IPv4 directive must format an IPv4: '%s'", l_out);

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_ip6;
	__util$faol ("!%6", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "2001:db8::1"), "the IPv6 directive must format the RFC 5952 canonical form: '%s'", l_out);

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_uu;
	__util$faol ("!%U", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "550e8400-e29b-41d4-a716-446655440000"), "the UUID directive must format a UUID: '%s'", l_out);

	/* RFC 5952 corner cases: the loopback, the all-zero address, a tie between
	** two runs (the leftmost is compressed), and lone zero groups (kept as-is).
	*/
	{
	unsigned char l_lo  [16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 };
	unsigned char l_any [16] = { 0 };
	unsigned char l_tie [16] = { 0x20,0x01,0,0,0,0,0,1,0,0,0,0,0,1,0,1 };
	unsigned char l_one [16] = { 0x20,0x01,0x0d,0xb8,0,1,0,2,0,3,0,4,0,5,0,6 };

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_lo;
	__util$faol ("!%6", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "::1"), "the loopback must compress to ::1: '%s'", l_out);

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_any;
	__util$faol ("!%6", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "::"), "the all-zero address must compress to :: alone: '%s'", l_out);

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_tie;
	__util$faol ("!%6", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "2001::1:0:0:1:1"), "on a tie the leftmost run must be compressed: '%s'", l_out);

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_one;
	__util$faol ("!%6", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "2001:db8:1:2:3:4:5:6"), "a lone zero group must not be compressed: '%s'", l_out);
	}

	/* An IPv4-mapped address is shown in the RFC 5952 par.5 mixed notation, and
	** every vector above renders byte-identically to inet_ntop(AF_INET6).
	*/
	{
	unsigned char l_map [16] = { 0,0,0,0,0,0,0,0,0,0,0xff,0xff,192,168,1,10 };
	unsigned char *l_all [] = { l_ip6, l_map };
	char	l_ref [64];
	int	l_i;

	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_map;
	__util$faol ("!%6", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "::ffff:192.168.1.10"), "an IPv4-mapped address must use the mixed notation: '%s'", l_out);

	for ( l_i = 0; l_i < 2; l_i++ )
		{
		l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t)(uintptr_t) l_all[l_i];
		__util$faol ("!%6", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
		inet_ntop (AF_INET6, l_all[l_i], l_ref, sizeof(l_ref));
		CHECK( 0 == strcmp(l_out, l_ref), "the IPv6 directive must match inet_ntop: '%s' vs '%s'", l_out, l_ref);
		}
	}

	/* Embedded in a larger control string, alongside a number directive. */
	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0;
	l_p[0] = (fao_prm_t)(uintptr_t) l_ip4; l_p[1] = (fao_prm_t) 42;
	__util$faol ("addr !%I port !UL", &l_len, &l_d, l_p, 2); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "addr 192.168.1.10 port 42"), "an address directive mixes with a number: '%s'", l_out);

	/* A NULL pointer must be safe. */
	l_d = (fao_desc_t) $FAO_DESC(l_out); l_len = 0; l_p[0] = (fao_prm_t) 0;
	__util$faol ("!%M", &l_len, &l_d, l_p, 1); l_out[l_len] = '\0';
	CHECK( 0 == strcmp(l_out, "(null)"), "a NULL address must yield (null): '%s'", l_out);

	TEST_DONE();
}
