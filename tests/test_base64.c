/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  __util$base64() and __util$unbase64().  The RFC 4648 test
**	vectors pin the encoding; a random round-trip over every length from
**	zero up covers the three tail shapes; the rest is what the decoder must
**	refuse and what a too-small buffer must still report.
**
**  MODULE:  test_base64.c
**--
*/
#define __MODULE__ "B64T$"

#include <string.h>
#include <stdlib.h>
#include "test_harness.h"
#include "utility_routines.h"
#include "starlet.h"

static const struct { const char *plain, *coded; } s_rfc [] = {
	{ "",		""		},
	{ "f",		"Zg=="		},
	{ "fo",		"Zm8="		},
	{ "foo",	"Zm9v"		},
	{ "foob",	"Zm9vYg=="	},
	{ "fooba",	"Zm9vYmE="	},
	{ "foobar",	"Zm9vYmFy"	},
};

int main (void)
{
char	l_enc [512], l_dec [512];
unsigned char l_raw [200], l_back [200];
int	l_i, l_j, l_n, l_len, l_status;

	printf("== base64 ==\n");

	/* The RFC 4648 vectors, both ways */
	for ( l_i = 0; l_i < (int) (sizeof(s_rfc) / sizeof(s_rfc[0])); l_i++ )
		{
		l_status = __util$base64((void *) s_rfc[l_i].plain, (int) strlen(s_rfc[l_i].plain), l_enc, sizeof(l_enc), &l_len);
		CHECK( (1 & l_status) && (l_len == (int) strlen(s_rfc[l_i].coded)) && !memcmp(l_enc, s_rfc[l_i].coded, l_len),
			"encode '%s' -> '%s' (got '%.*s')", s_rfc[l_i].plain, s_rfc[l_i].coded, l_len, l_enc);
		CHECK( l_enc[l_len] == '\0', "encode '%s': courtesy NUL after the text", s_rfc[l_i].plain);

		l_status = __util$unbase64((void *) s_rfc[l_i].coded, (int) strlen(s_rfc[l_i].coded), l_dec, sizeof(l_dec), &l_len);
		CHECK( (1 & l_status) && (l_len == (int) strlen(s_rfc[l_i].plain)) && !memcmp(l_dec, s_rfc[l_i].plain, l_len),
			"decode '%s' -> '%s'", s_rfc[l_i].coded, s_rfc[l_i].plain);
		}

	/* Unpadded input is accepted */
	l_status = __util$unbase64("Zg", 2, l_dec, sizeof(l_dec), &l_len);
	CHECK( (1 & l_status) && (l_len == 1) && (l_dec[0] == 'f'), "unpadded 'Zg' decodes to 'f'");
	l_status = __util$unbase64("Zm8", 3, l_dec, sizeof(l_dec), &l_len);
	CHECK( (1 & l_status) && (l_len == 2) && !memcmp(l_dec, "fo", 2), "unpadded 'Zm8' decodes to 'fo'");

	/* Random round-trip over every length: all three tail shapes, every octet value */
	srand(20260910);

	for ( l_n = 0; l_n < (int) sizeof(l_raw); l_n++ )
		{
		for ( l_j = 0; l_j < l_n; l_j++ )
			l_raw[l_j] = (unsigned char) rand();

		l_status = __util$base64(l_raw, l_n, l_enc, sizeof(l_enc), &l_len);
		CHECK( (1 & l_status) && (l_len == 4 * ((l_n + 2) / 3)), "encode %d octets -> %d chars", l_n, l_len);

		l_status = __util$unbase64(l_enc, l_len, l_back, sizeof(l_back), &l_j);
		CHECK( (1 & l_status) && (l_j == l_n) && !memcmp(l_raw, l_back, l_n), "round trip of %d octets", l_n);
		}

	/* A buffer too small: an error, and the needed length is still reported */
	l_status = __util$base64("foobar", 6, l_enc, 7, &l_len);
	CHECK( !(1 & l_status) && (l_len == 8), "encode into 7 octets fails and reports 8 needed");
	l_status = __util$base64("foobar", 6, l_enc, 8, &l_len);
	CHECK( (1 & l_status) && (l_len == 8), "encode into exactly 8 octets succeeds (no NUL, no overrun)");
	l_status = __util$unbase64("Zm9vYmFy", 8, l_dec, 5, &l_len);
	CHECK( !(1 & l_status) && (l_len == 6), "decode into 5 octets fails and reports 6 needed");

	/* What the decoder refuses */
	l_status = __util$unbase64("Zm9v\nYmFy", 9, l_dec, sizeof(l_dec), &l_len);
	CHECK( !(1 & l_status), "an octet outside the alphabet (LF) is refused");
	l_status = __util$unbase64("Zm=vYmFy", 8, l_dec, sizeof(l_dec), &l_len);
	CHECK( !(1 & l_status), "a '=' in the middle is refused");
	l_status = __util$unbase64("Zm9vY", 5, l_dec, sizeof(l_dec), &l_len);
	CHECK( !(1 & l_status), "a dangling single sextet is refused");
	l_status = __util$unbase64("Zm9v=", 5, l_dec, sizeof(l_dec), &l_len);
	CHECK( !(1 & l_status), "a '=' that completes nothing is refused");
	l_status = __util$unbase64("Zg===", 5, l_dec, sizeof(l_dec), &l_len);
	CHECK( !(1 & l_status), "three '=' are refused");
	l_status = __util$unbase64("Zm9v", 4, l_dec, sizeof(l_dec), NULL);
	CHECK( !(1 & l_status), "a NULL dstlen is refused");

	/* The shape the proxy sees: "user:password" for Proxy-Authorization */
	l_status = __util$base64("Aladdin:open sesame", 19, l_enc, sizeof(l_enc), &l_len);
	CHECK( (1 & l_status) && !strcmp(l_enc, "QWxhZGRpbjpvcGVuIHNlc2FtZQ=="), "RFC 7617 example encodes");
	l_status = __util$unbase64(l_enc, l_len, l_dec, sizeof(l_dec), &l_len);
	CHECK( (1 & l_status) && (l_len == 19) && !memcmp(l_dec, "Aladdin:open sesame", 19), "...and decodes");

	TEST_DONE();
}
