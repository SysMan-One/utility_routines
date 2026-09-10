#!/bin/sh
#
#  Build and run the utility_routines regression tests without CMake.
#
#  Usage:   sh tests/run.sh        (from anywhere)
#  Honours: CC, CFLAGS
#
#  Exits non-zero if any test fails, so it is usable from a build pipeline.
#
set -eu

CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=gnu11 -O2 -Wall -Wextra}

HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

SRC="$HERE/utility_routines.c $HERE/cli_routines.c $HERE/starlet.c $HERE/avproto.c"
rc=0

for t in status_convention cli_features tlv cli_help log_overflow setlogfd putmsg_fao cli_nested cli_filepath fao_addr log_prefix pattern_match log_fao base64
do
	printf '\n>>> test_%s\n' "$t"
	# shellcheck disable=SC2086
	$CC $CFLAGS -I"$HERE" "$HERE/tests/test_$t.c" $SRC -o "$OUT/test_$t"
	"$OUT/test_$t" || rc=1
done

exit $rc
