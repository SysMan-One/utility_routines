/*
**++
**  FACILITY:  utility_routines - regression tests
**
**  ABSTRACT:  __util$crc32c() - the IEEE CRC-32 - against a bitwise
**	reference: the check values, every length up to 2048 at every
**	alignment 0..15, a run continued from a previous result at every
**	split point, a large buffer.  Run once as it stands (the fastest
**	engine of the processor) and once under STARLET_CRC32=SOFTWARE (the
**	tables), so every engine there is gets pinned to the same values.
**
**  MODULE:  test_crc32.c
**--
*/
#define __MODULE__ "CRCT$"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "test_harness.h"
#include "utility_routines.h"

/* One bit at a time: slow, obvious, independent of every engine */
static unsigned int	s_ref (unsigned int a_crc, const unsigned char *a_p, size_t a_len)
{
unsigned int	l_crc = a_crc ^ ~0U;

	while ( a_len-- )
		{
		l_crc ^= *a_p++;

		for ( int k = 0; k < 8; k++ )
			l_crc = (l_crc >> 1) ^ (0xEDB88320U & (0U - (l_crc & 1)));
		}

	return	l_crc ^ ~0U;
}

int main (void)
{
const char	*l_env = getenv("STARLET_CRC32");
unsigned char	*l_buf;
size_t		l_big = 4 << 20;
int		l_bad;

	printf("== crc32 (%s) ==\n", l_env ? l_env : "fastest engine");

	CHECK(__util$crc32c(0, "123456789", 9) == 0xCBF43926U, "check value: %08X", __util$crc32c(0, "123456789", 9));
	CHECK(__util$crc32c(0, "The quick brown fox jumps over the lazy dog", 43) == 0x414FA339U, "quick brown fox");
	CHECK(__util$crc32c(0, "", 0) == 0, "empty");

	if ( !(l_buf = malloc(l_big + 64)) )
		return	1;

	srand(12345);

	for ( size_t i = 0; i < l_big + 64; i++ )
		l_buf [i] = (unsigned char) rand();

	/* Every length at every alignment */
	l_bad = 0;

	for ( size_t l_off = 0; l_off < 16; l_off++ )
		for ( size_t l_len = 0; l_len <= 2048; l_len++ )
			if ( __util$crc32c(0, l_buf + l_off, l_len) != s_ref(0, l_buf + l_off, l_len) )
				l_bad++;

	CHECK(!l_bad, "%d length/alignment cases differ from the reference", l_bad);

	/* Continued from a previous result, split anywhere */
	l_bad = 0;

	for ( size_t l_cut = 0; l_cut <= 1000; l_cut++ )
		if ( __util$crc32c(__util$crc32c(0, l_buf + 3, l_cut), l_buf + 3 + l_cut, 1000 - l_cut) != s_ref(0, l_buf + 3, 1000) )
			l_bad++;

	CHECK(!l_bad, "%d split points differ from the reference", l_bad);

	/* A large buffer, odd length, odd start */
	CHECK(__util$crc32c(0x12345678U, l_buf + 1, l_big - 7) == s_ref(0x12345678U, l_buf + 1, l_big - 7), "4 MB buffer");

	/* For information only: how fast this engine is */
	{
	struct timespec	l_t0, l_t1;
	unsigned int	l_crc = 0;
	double		l_s;

	clock_gettime(CLOCK_MONOTONIC, &l_t0);

	for ( int i = 0; i < 64; i++ )
		l_crc = __util$crc32c(l_crc, l_buf, l_big);

	clock_gettime(CLOCK_MONOTONIC, &l_t1);
	l_s = (double) (l_t1.tv_sec - l_t0.tv_sec) + (double) (l_t1.tv_nsec - l_t0.tv_nsec) / 1e9;

	printf("  %.2f GB/s (%08X)\n", (64.0 * (double) l_big) / l_s / 1e9, l_crc);
	}

	free(l_buf);

	TEST_DONE();
}
