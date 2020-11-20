#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-2020 Intel Corporation. All rights reserved.

blockdev=""
rc=77

. $(dirname $0)/common

force_raw()
{
	raw="$1"
	$NDCTL disable-namespace "$dev"
	echo "$raw" > "/sys/bus/nd/devices/$dev/force_raw"
	$NDCTL enable-namespace "$dev"
	echo "Set $dev to raw mode: $raw"
	if [[ "$raw" == "1" ]]; then
		raw_bdev=${blockdev}
		test -b "/dev/$raw_bdev"
	else
		raw_bdev=""
	fi
}

check_min_kver "4.20" || do_skip "may lack PFN metadata error handling"

set -e
trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all

rc=1

# create a fsdax namespace and clear errors (if any)
dev="x"
json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem -m fsdax)
eval "$(echo "$json" | json2var)"
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1

force_raw 1
if read -r sector len < "/sys/block/$raw_bdev/badblocks"; then
	dd of=/dev/$raw_bdev if=/dev/zero oflag=direct bs=512 seek="$sector" count="$len"
fi
force_raw 0

# find dataoff from sb
force_raw 1
doff=$(hexdump -s $((4096 + 56)) -n 4 "/dev/$raw_bdev" | head -1 | cut -d' ' -f2-)
doff=$(tr -d ' ' <<< "0x${doff#* }${doff%% *}")
printf "pfn dataoff: %x\n" "$doff"
dblk="$((doff/512))"

metaoff="0x2000"
mblk="$((metaoff/512))"

# inject in the middle of the struct page area
bb_inj=$(((dblk - mblk)/2))
$NDCTL inject-error --block="$bb_inj" --count=32 $dev
$NDCTL start-scrub $NFIT_TEST_BUS0 && $NDCTL wait-scrub $NFIT_TEST_BUS0

# after probe from the enable-namespace, the error should've been cleared
force_raw 0
force_raw 1
if read -r sector len < "/sys/block/$raw_bdev/badblocks"; then
	false
fi

_cleanup
exit 0
