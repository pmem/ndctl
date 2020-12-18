#!/bin/bash -E
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

dev=""
mode=""
size=""
sector_size=""
blockdev=""
bs=4096
rc=77

. $(dirname $0)/common

trap 'err $LINENO' ERR

# sample json:
# {
#   "dev":"namespace5.0",
#   "mode":"sector",
#   "size":32440320,
#   "uuid":"51805176-e124-4635-ae17-0e6a4a16671a",
#   "sector_size":4096,
#   "blockdev":"pmem5s"
# }

check_min_kver "4.14" || do_skip "may not support badblocks clearing on pmem via btt"

create()
{
	json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem -m sector)
	rc=2
	eval "$(echo "$json" | json2var)"
	[ -n "$dev" ] || err "$LINENO"
	[ "$mode" = "sector" ] || err "$LINENO"
	[ -n "$size" ] || err "$LINENO"
	[ -n "$sector_size" ] || err "$LINENO"
	[ -n "$blockdev" ] || err "$LINENO"
	[ $size -gt 0 ] || err "$LINENO"
}

reset()
{
	$NDCTL disable-region -b $NFIT_TEST_BUS0 all
	$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
	$NDCTL enable-region -b $NFIT_TEST_BUS0 all
}

# re-enable the BTT namespace, and do IO to it in an attempt to
# verify it still comes up ok, and functions as expected
post_repair_test()
{
	echo "${FUNCNAME[0]}: I/O to BTT namespace"
	test -b /dev/$blockdev
	dd if=/dev/urandom of=test-bin bs=$sector_size count=$((size/sector_size)) > /dev/null 2>&1
	dd if=test-bin of=/dev/$blockdev bs=$sector_size count=$((size/sector_size)) > /dev/null 2>&1
	dd if=/dev/$blockdev of=test-bin-read bs=$sector_size count=$((size/sector_size)) > /dev/null 2>&1
	diff test-bin test-bin-read
	rm -f test-bin*
	echo "done"
}

test_normal()
{
	echo "=== ${FUNCNAME[0]} ==="
	# disable the namespace
	$NDCTL disable-namespace $dev
	$NDCTL check-namespace $dev
	$NDCTL enable-namespace $dev
	post_repair_test
}

test_force()
{
	echo "=== ${FUNCNAME[0]} ==="
	$NDCTL check-namespace --force $dev
	post_repair_test
}

set_raw()
{
	$NDCTL disable-namespace $dev
	echo -n "set raw_mode: "
	echo 1 | tee /sys/bus/nd/devices/$dev/force_raw
	$NDCTL enable-namespace $dev
	raw_bdev="${blockdev%%s}"
	test -b /dev/$raw_bdev
	raw_size="$(cat /sys/bus/nd/devices/$dev/size)"
}

unset_raw()
{
	$NDCTL disable-namespace $dev
	echo -n "set raw_mode: "
	echo 0 | tee /sys/bus/nd/devices/$dev/force_raw
	$NDCTL enable-namespace $dev
	raw_bdev=""
}

test_bad_info2()
{
	echo "=== ${FUNCNAME[0]} ==="
	set_raw
	seek="$((raw_size/bs - 1))"
	echo "wiping info2 block (offset = $seek blocks)"
	dd if=/dev/zero of=/dev/$raw_bdev bs=$bs count=1 seek=$seek
	unset_raw
	$NDCTL disable-namespace $dev
	$NDCTL check-namespace $dev 2>&1 | grep "info2 needs to be restored"
	$NDCTL check-namespace --repair $dev
	$NDCTL enable-namespace $dev
	post_repair_test
}

test_bad_info()
{
	echo "=== ${FUNCNAME[0]} ==="
	set_raw
	echo "wiping info block"
	dd if=/dev/zero of=/dev/$raw_bdev bs=$bs count=2 seek=0
	unset_raw
	$NDCTL disable-namespace $dev
	$NDCTL check-namespace $dev 2>&1 | grep -E "info block at offset .* needs to be restored"
	$NDCTL check-namespace --repair $dev
	$NDCTL enable-namespace $dev
	post_repair_test
}

test_bitmap()
{
	echo "=== ${FUNCNAME[0]} ==="
	reset && create
	set_raw
	# scribble over the last 4K of the map
	rm -f /tmp/scribble
	for (( i=0 ; i<512 ; i++ )); do
		echo -n -e \\x1e\\x1e\\x00\\xc0\\x1e\\x1e\\x00\\xc0 >> /tmp/scribble
	done
	seek="$((raw_size/bs - (256*64/bs) - 2))"
	echo "scribbling over map entries (offset = $seek blocks)"
	dd if=/tmp/scribble of=/dev/$raw_bdev bs=$bs seek=$seek
	rm -f /tmp/scribble
	unset_raw
	$NDCTL disable-namespace $dev
	$NDCTL check-namespace $dev 2>&1 | grep "bitmap error"
	# This is not repairable
	reset && create
}

do_tests()
{
	test_normal
	test_force
	test_bad_info2
	test_bad_info
	test_bitmap
}

# setup (reset nfit_test dimms, create the BTT namespace)
modprobe nfit_test
rc=1
reset && create
do_tests
reset
_cleanup
exit 0
