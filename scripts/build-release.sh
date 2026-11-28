#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Keep the firmware baseline independent of host-native compiler flags.
set -eu
case "$1" in
    portable) flags='-O2 -march=x86-64 -mtune=generic';;
    aggressive) flags='-O3 -march=native -flto';;
    *) exit 2;;
esac
test "$(git hash-object Libraries/include/infiltratr/filesystem_module_v2.h)" = 90730b68a53ecf6a6e304c789501f29c17c728c7
make clean
# The external formatter is an independent test oracle, never a runtime backend.
command -v genisoimage >/dev/null || { echo 'genisoimage is required for UDF independent-media testing' >&2; exit 1; }
fixture=$(mktemp -d)
trap 'rm -rf -- "$fixture"' EXIT HUP INT TERM
mkdir -p "$fixture/tree/dir"
printf yes > "$fixture/tree/dir/child"
dd if=/dev/urandom of="$fixture/tree/proof.bin" bs=1 count=65573 status=none
genisoimage -quiet -udf -V V2UDF -o "$fixture/media.img" "$fixture/tree"
size=$(stat -c %s "$fixture/media.img")
truncate -s "$(( (size+4095)/4096*4096 ))" "$fixture/media.img"
sha256sum "$fixture/media.img" > "$fixture/before"
make CFLAGS="$flags" check MEDIA="$fixture/media.img" PROOF="$fixture/tree/proof.bin"
sha256sum -c "$fixture/before"
# QNX4's check target builds the hosted module, while the other providers
# also test their carrier. Explicitly request the carrier if not present.
test -s build/carrier/UDF.so || make CFLAGS="$flags" carrier
sh scripts/package.sh "$2"
