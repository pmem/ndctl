#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

dev=""
size=""
blockdev=""
rc=77

BASE=$(dirname $0)
. $BASE/common

trap 'err $LINENO' ERR

# sample json:
#{
#  "dev":"namespace7.0",
#  "mode":"fsdax",
#  "size":"60.00 MiB (62.92 MB)",
#  "uuid":"f1baa71a-d165-4da4-bb6a-083a2b0e6469",
#  "blockdev":"pmem7",
#}

create()
{
	json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem -m sector)
	rc=2
	eval "$(echo "$json" | json2var)"
	[ -n "$dev" ] || err "$LINENO"
	[ -n "$size" ] || err "$LINENO"
	[ -n "$blockdev" ] || err "$LINENO"
	[ $size -gt 0 ] || err "$LINENO"
	bttdev=$(cat /sys/bus/nd/devices/$dev/holder)
	[ -n "$bttdev" ] || err "$LINENO"
	if [ ! -e /sys/kernel/debug/btt/$bttdev/arena0/log_index_0 ]; then
		do_skip "seems to be missing the BTT compatibility fixes, skipping."
	fi
}

reset()
{
	$NDCTL disable-region -b $NFIT_TEST_BUS0 all
	$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
	$NDCTL enable-region -b $NFIT_TEST_BUS0 all
}

verify_idx()
{
	idx0="$1"
	idx1="$2"

	# check debugfs is mounted
	if ! grep -qE "debugfs" /proc/mounts; then
		mount -t debugfs none /sys/kernel/debug
	fi

	test $(cat /sys/kernel/debug/btt/$bttdev/arena0/log_index_0) -eq "$idx0"
	test $(cat /sys/kernel/debug/btt/$bttdev/arena0/log_index_1) -eq "$idx1"
}

do_random_io()
{
	local bdev="$1"

	dd if=/dev/urandom of="$bdev" bs=4096 count=32 seek=0 &
	dd if=/dev/urandom of="$bdev" bs=4096 count=32 seek=32 &
	dd if=/dev/urandom of="$bdev" bs=4096 count=32 seek=64 &
	dd if=/dev/urandom of="$bdev" bs=4096 count=32 seek=128 &
	dd if=/dev/urandom of="$bdev" bs=4096 count=32 seek=256 &
	dd if=/dev/urandom of="$bdev" bs=4096 count=32 seek=512 &
	dd if=/dev/urandom of="$bdev" bs=4096 count=32 seek=1024 &
	dd if=/dev/urandom of="$bdev" bs=4096 count=32 seek=2048 &
	wait
}

cycle_ns()
{
	local ns="$1"

	$NDCTL disable-namespace $ns
	$NDCTL enable-namespace $ns
}

force_raw()
{
	raw="$1"
	$NDCTL disable-namespace "$dev"
	echo "$raw" > "/sys/bus/nd/devices/$dev/force_raw"
	$NDCTL enable-namespace "$dev"
	echo "Set $dev to raw mode: $raw"
	if [[ "$raw" == "1" ]]; then
		raw_bdev=${blockdev%s}
		test -b "/dev/$raw_bdev"
	else
		raw_bdev=""
	fi
}

copy_xxd_img()
{
	local bdev="$1"
	local xxd_patch="$BASE/btt-pad-compat.xxd"

	test -s "$xxd_patch"
	test -b "$bdev"
	xxd -r "$xxd_patch" "$bdev"
}

create_oldfmt_ns()
{
	# create null-uuid namespace, note that this requires a kernel
	# that supports a raw namespace with a 4K sector size, prior to
	# v4.13 raw namespaces are limited to 512-byte sector size.
	rc=77
	json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -s 64M -t pmem -m raw -l 4096 -u 00000000-0000-0000-0000-000000000000)
	rc=2
	eval "$(echo "$json" | json2var)"
	[ -n "$dev" ] || err "$LINENO"
	[ -n "$size" ] || err "$LINENO"
	[ $size -gt 0 ] || err "$LINENO"

	# reconfig it to sector mode
	json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -e $dev -m sector --force)
	eval "$(echo "$json" | json2var)"
	[ -n "$dev" ] || err "$LINENO"
	[ -n "$size" ] || err "$LINENO"
	[ -n "$blockdev" ] || err "$LINENO"
	[ $size -gt 0 ] || err "$LINENO"
	bttdev=$(cat /sys/bus/nd/devices/$dev/holder)
	[ -n "$bttdev" ] || err "$LINENO"

	rc=1
	# copy old-padding-format btt image, and try to re-enable the resulting btt
	force_raw 1
	copy_xxd_img "/dev/$raw_bdev"
	force_raw 0
	test -b "/dev/$blockdev"
}

ns_info_wipe()
{
	force_raw 1
	dd if=/dev/zero of=/dev/$raw_bdev bs=4096 count=2
}

do_tests()
{
	# regular btt
	create
	verify_idx 0 1

	# do io, and cycle namespace, verify indices
	do_random_io "/dev/$blockdev"
	cycle_ns "$dev"
	verify_idx 0 1

	# do the same with an old format namespace
	reset
	create_oldfmt_ns
	verify_idx 0 2

	# do io, and cycle namespace, verify indices
	do_random_io "/dev/$blockdev"
	cycle_ns "$dev"
	verify_idx 0 2

	# rewrite log using ndctl, verify conversion to new format
	$NDCTL check-namespace --rewrite-log --repair --force --verbose $dev
	do_random_io "/dev/$blockdev"
	cycle_ns "$dev"
	verify_idx 0 1

	# check-namespace again to make sure everything is ok
	$NDCTL check-namespace --force --verbose $dev

	# the old format btt metadata was created with a null parent uuid,
	# making it 'stickier' than a normally created btt. Be sure to clean
	# it up by wiping the info block
	ns_info_wipe
}

modprobe nfit_test
check_prereq xxd
rc=1
reset
do_tests
reset
_cleanup
exit 0
