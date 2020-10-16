#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

dev=""
size=""
blockdev=""
rc=77
err_block=42
err_count=8

. $(dirname $0)/common

trap 'err $LINENO' ERR

# sample json:
#{
#  "dev":"namespace7.0",
#  "mode":"fsdax",
#  "size":"60.00 MiB (62.92 MB)",
#  "uuid":"f1baa71a-d165-4da4-bb6a-083a2b0e6469",
#  "blockdev":"pmem7",
#}

check_min_kver "4.15" || do_skip "kernel $KVER may not support error injection"

create()
{
	json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem --align=4k)
	rc=2
	eval "$(echo "$json" | json2var)"
	[ -n "$dev" ] || err "$LINENO"
	[ -n "$size" ] || err "$LINENO"
	[ -n "$blockdev" ] || err "$LINENO"
	[ $size -gt 0 ] || err "$LINENO"
}

reset()
{
	$NDCTL disable-region -b $NFIT_TEST_BUS0 all
	$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
	$NDCTL enable-region -b $NFIT_TEST_BUS0 all
}

check_status()
{
	local sector="$1"
	local count="$2"

	json="$($NDCTL inject-error --status $dev)"
	[[ "$sector" == "$(jq ".badblocks[0].block" <<< "$json")" ]]
	[[ "$count" == "$(jq ".badblocks[0].count" <<< "$json")" ]]
}

do_tests()
{
	# inject without notification
	$NDCTL inject-error --block=$err_block --count=$err_count --no-notify $dev
	check_status "$err_block" "$err_count"
	if read -r sector len < /sys/block/$blockdev/badblocks; then
		# fail if reading badblocks returns data
		echo "fail: $LINENO" && exit 1
	fi

	# clear via err-inj-clear
	$NDCTL inject-error --block=$err_block --count=$err_count --uninject $dev
	check_status

	# inject normally
	$NDCTL inject-error --block=$err_block --count=$err_count $dev
	$NDCTL start-scrub $NFIT_TEST_BUS0 && $NDCTL wait-scrub $NFIT_TEST_BUS0
	check_status "$err_block" "$err_count"
	if read -r sector len < /sys/block/$blockdev/badblocks; then
		test "$sector" -eq "$err_block"
		test "$len" -eq "$err_count"
	fi

	# clear via write
	dd if=/dev/zero of=/dev/$blockdev bs=512 count=$err_count seek=$err_block oflag=direct
	if read -r sector len < /sys/block/$blockdev/badblocks; then
		# fail if reading badblocks returns data
		echo "fail: $LINENO" && exit 1
	fi
	check_status
}

modprobe nfit_test
rc=1
reset && create
do_tests
reset
_cleanup
exit 0
