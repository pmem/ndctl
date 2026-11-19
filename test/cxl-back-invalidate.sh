#!/bin/bash
# SPDX-License-Identifier: GPL-2.0

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test

CXLDEV=/sys/bus/cxl/devices

# A Back-Invalidate capable window is advertised by the root decoder as
# cap_back_invalidate. Without one the kernel has no BI support, or
# cxl_test predates the mock BI topology.
find_bi_root()
{
	local d

	for d in $CXLDEV/decoder*; do
		[ -e "$d/cap_back_invalidate" ] || continue
		[ "$(cat $d/cap_back_invalidate)" = 1 ] || continue
		basename "$d"
		return
	done
}

bi_root=$(find_bi_root)
if [ -z "$bi_root" ]; then
	if check_min_kver "7.5"; then
		rc=1
		err "$LINENO"
	fi
	do_skip "test requires a back-invalidate capable window"
fi

# Replay support is exposed by cxl_acpi after cxl_test loads
if [ ! -e /sys/bus/platform/devices/cxl_acpi.0/decoder_reset_preserve_registry ]; then
	do_skip "test requires decoder registry replay support"
fi

rc=1

# A region under a BI window is HDM-DB: device-coherent, with the BI bit
# programmed into the endpoint decoder at commit. Only memdevs below the
# host bridge the window targets can attach to it.
create_bi_region()
{
	local port_dev mem

	port_dev=$("$CXL" list -T -d "$bi_root" |
		jq -r ".[] | .targets | .[0].target")
	[[ $port_dev ]] || return 1

	for mem in $("$CXL" list -M -p "$port_dev" | jq -r ".[].memdev"); do
		BI_REGION=$("$CXL" create-region -d "$bi_root" -m "$mem" \
			-t ram 2>/dev/null | jq -r ".region")
		[[ $BI_REGION && $BI_REGION != null ]] || continue

		BI_DECODER=$("$CXL" list -R --targets -r "$BI_REGION" |
			jq -r ".[0].mappings | .[0].decoder")
		[[ $BI_DECODER && $BI_DECODER != null ]] || return 1
		return 0
	done

	echo "no memdev below $port_dev could back a region on $bi_root"
	return 1
}

BI_REGION=""
BI_DECODER=""
create_bi_region || err "$LINENO"

# The endpoint decoder inherits the window's coherency model: HDM-DB is
# device-coherent with the BI bit set.
[ "$(cat $CXLDEV/$BI_DECODER/back_invalidate)" = "1" ] || err "$LINENO"
[ "$(cat $CXLDEV/$BI_DECODER/target_type)" = "accelerator" ] || err "$LINENO"

# cxl list reports the same BI state as sysfs
"$CXL" list -D -d "$bi_root" |
	jq -e '.[0].back_invalidate_capable == true' >/dev/null || err "$LINENO"
"$CXL" list -D -d "$BI_DECODER" |
	jq -e '.[0].back_invalidate == true' >/dev/null || err "$LINENO"

[ "$(cat $CXLDEV/$BI_REGION/commit)" = "1" ] || err "$LINENO"

# The committed HDM-DB region must survive a cxl_acpi rebind and come
# back with its coherency model intact.
replay_regions || err "$LINENO"

bi_root=$(find_bi_root)
[ -n "$bi_root" ] || err "$LINENO"

replayed=""
for r in $CXLDEV/region*; do
	[ -d "$r" ] || continue
	[ "$(cat "$r/commit" 2>/dev/null)" = "1" ] || continue
	readlink -f "$r" | grep -q "/$bi_root/" || continue
	[ -z "$replayed" ] || err "$LINENO"
	replayed=$(basename "$r")
done
[ -n "$replayed" ] || err "$LINENO"

# decoder ids are not stable across the rebind, so ask the replayed
# region which endpoint decoder it came back on.
replayed_dec=$("$CXL" list -R --targets -r "$replayed" |
	jq -r ".[0].mappings | .[0].decoder")
[[ $replayed_dec && $replayed_dec != null ]] || err "$LINENO"
[ "$(cat $CXLDEV/$replayed_dec/back_invalidate)" = "1" ] || err "$LINENO"

"$CXL" destroy-region -f -b "$CXL_TEST_BUS" "$replayed"

check_dmesg "$LINENO"

modprobe -r cxl_test
