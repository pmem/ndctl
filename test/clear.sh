#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

set -e

rc=77

. $(dirname $0)/common

check_min_kver "4.6" || do_skip "lacks clear poison support"

trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all

rc=1

# create pmem
dev="x"
json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem -m raw)
eval $(echo $json | json2var)
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1
[ $mode != "raw" ] && echo "fail: $LINENO" && exit 1

# inject errors in the middle of the namespace, verify that reading fails
err_sector="$(((size/512) / 2))"
err_count=8
if ! read sector len < /sys/block/$blockdev/badblocks; then
	$NDCTL inject-error --block="$err_sector" --count=$err_count $dev
	$NDCTL start-scrub $NFIT_TEST_BUS0 && $NDCTL wait-scrub $NFIT_TEST_BUS0
fi
read sector len < /sys/block/$blockdev/badblocks
[ $((sector * 2)) -ne $((size /512)) ] && echo "fail: $LINENO" && exit 1
if dd if=/dev/$blockdev of=/dev/null iflag=direct bs=512 skip=$sector count=$len; then
	echo "fail: $LINENO" && exit 1
fi

size_raw=$size
sector_raw=$sector

# convert pmem to fsdax mode
json=$($NDCTL create-namespace -m fsdax -f -e $dev)
eval $(echo $json | json2var)
[ $mode != "fsdax" ] && echo "fail: $LINENO" && exit 1

# check for errors relative to the offset injected by the pfn device
read sector len < /sys/block/$blockdev/badblocks
[ $((sector_raw - sector)) -ne $(((size_raw - size) / 512)) ] && echo "fail: $LINENO" && exit 1

# check that writing clears the errors
if ! dd of=/dev/$blockdev if=/dev/zero oflag=direct bs=512 seek=$sector count=$len; then
	echo "fail: $LINENO" && exit 1
fi

if read sector len < /sys/block/$blockdev/badblocks; then
	# fail if reading badblocks returns data
	echo "fail: $LINENO" && exit 1
fi

if check_min_kver "4.9"; then
	# check for re-appearance of stale badblocks from poison_list
	$NDCTL disable-region -b $NFIT_TEST_BUS0 all
	$NDCTL enable-region -b $NFIT_TEST_BUS0 all

	# since we have cleared the errors, a disable/reenable shouldn't bring them back
	if read sector len < /sys/block/$blockdev/badblocks; then
		# fail if reading badblocks returns data
		echo "fail: $LINENO" && exit 1
	fi
fi

_cleanup

exit 0
