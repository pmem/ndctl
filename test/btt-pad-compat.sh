#!/bin/bash -Ex

# Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

[ -f "../ndctl/ndctl" ] && [ -x "../ndctl/ndctl" ] && ndctl="../ndctl/ndctl"
[ -f "./ndctl/ndctl" ] && [ -x "./ndctl/ndctl" ] && ndctl="./ndctl/ndctl"
[ -z "$ndctl" ] && echo "Couldn't find an ndctl binary" && exit 1
bus="nfit_test.0"
json2var="s/[{}\",]//g; s/:/=/g"
dev=""
size=""
blockdev=""
rc=77

trap 'err $LINENO' ERR

# sample json:
#{
#  "dev":"namespace7.0",
#  "mode":"fsdax",
#  "size":"60.00 MiB (62.92 MB)",
#  "uuid":"f1baa71a-d165-4da4-bb6a-083a2b0e6469",
#  "blockdev":"pmem7",
#}

# $1: Line number
# $2: exit code
err()
{
	[ -n "$2" ] && rc="$2"
	echo "test/btt-pad-compat.sh: failed at line $1"
	exit "$rc"
}

check_prereq()
{
	if ! command -v "$1" >/dev/null; then
		echo "missing '$1', skipping.."
		exit "$rc"
	fi
}

create()
{
	json=$($ndctl create-namespace -b "$bus" -t pmem -m sector)
	eval "$(echo "$json" | sed -e "$json2var")"
	[ -n "$dev" ] || err "$LINENO" 2
	[ -n "$size" ] || err "$LINENO" 2
	[ -n "$blockdev" ] || err "$LINENO" 2
	[ $size -gt 0 ] || err "$LINENO" 2
	bttdev=$(cat /sys/bus/nd/devices/$dev/holder)
	[ -n "$bttdev" ] || err "$LINENO" 2
	if [ ! -e /sys/kernel/debug/btt/$bttdev/arena0/log_index_0 ]; then
		echo "kernel $(uname -r) seems to be missing the BTT compatibility fixes, skipping"
		exit 77
	fi
}

reset()
{
	$ndctl disable-region -b "$bus" all
	$ndctl zero-labels -b "$bus" all
	$ndctl enable-region -b "$bus" all
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

	$ndctl disable-namespace $ns
	$ndctl enable-namespace $ns
}

force_raw()
{
	raw="$1"
	$ndctl disable-namespace "$dev"
	echo "$raw" > "/sys/bus/nd/devices/$dev/force_raw"
	$ndctl enable-namespace "$dev"
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
	local xxd_patch="btt-pad-compat.xxd"

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
	json=$($ndctl create-namespace -b "$bus" -s 64M -t pmem -m raw -l 4096 -u 00000000-0000-0000-0000-000000000000)
	rc=1
	eval "$(echo "$json" | sed -e "$json2var")"
	[ -n "$dev" ] || err "$LINENO" 2
	[ -n "$size" ] || err "$LINENO" 2
	[ $size -gt 0 ] || err "$LINENO" 2

	# reconfig it to sector mode
	json=$($ndctl create-namespace -b "$bus" -e $dev -m sector --force)
	eval "$(echo "$json" | sed -e "$json2var")"
	[ -n "$dev" ] || err "$LINENO" 2
	[ -n "$size" ] || err "$LINENO" 2
	[ -n "$blockdev" ] || err "$LINENO" 2
	[ $size -gt 0 ] || err "$LINENO" 2
	bttdev=$(cat /sys/bus/nd/devices/$dev/holder)
	[ -n "$bttdev" ] || err "$LINENO" 2

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
	$ndctl check-namespace --rewrite-log --repair --force --verbose $dev
	do_random_io "/dev/$blockdev"
	cycle_ns "$dev"
	verify_idx 0 1

	# check-namespace again to make sure everything is ok
	$ndctl check-namespace --force --verbose $dev

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
exit 0
