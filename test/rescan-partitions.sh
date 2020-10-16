#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-2020 Intel Corporation. All rights reserved.

dev=""
size=""
blockdev=""
rc=77

. $(dirname $0)/common

trap 'err $LINENO' ERR

# sample json:
#{
#  "dev":"namespace5.0",
#  "mode":"sector",
#  "size":"60.00 MiB (62.92 MB)",
#  "uuid":"f1baa71a-d165-4da4-bb6a-083a2b0e6469",
#  "blockdev":"pmem5s",
#}

check_min_kver "4.16" || do_skip "may not contain fixes for partition rescanning"

check_prereq "parted"
check_prereq "blockdev"

reset()
{
	$NDCTL disable-region -b $NFIT_TEST_BUS0 all
	$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
	$NDCTL enable-region -b $NFIT_TEST_BUS0 all
}

test_mode()
{
	local mode="$1"

	# create namespace
	json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem -m "$mode")
	rc=2
	eval "$(echo "$json" | json2var)"
	[ -n "$dev" ] || err "$LINENO"
	[ -n "$size" ] || err "$LINENO"
	[ -n "$blockdev" ] || err "$LINENO"
	[ $size -gt 0 ] || err "$LINENO"

	rc=1
	# create partition
	parted --script /dev/$blockdev mklabel gpt mkpart primary 1MiB 10MiB

	# verify it is created
	sleep 1
	blockdev --rereadpt /dev/$blockdev
	sleep 1
	partdev="$(grep -Eo "${blockdev}.+" /proc/partitions)"
	test -b /dev/$partdev

	# cycle the namespace, and verify the partition is read
	# without needing to do a blockdev --rereadpt
	$NDCTL disable-namespace $dev
	$NDCTL enable-namespace $dev
	if [ -b /dev/$partdev ]; then
		echo "mode: $mode - partition read successful"
	else
		echo "mode: $mode - partition read failed"
		rc=1
		err "$LINENO"
	fi

	$NDCTL disable-namespace $dev
	$NDCTL destroy-namespace $dev
}

modprobe nfit_test
rc=1
reset
test_mode "raw"
test_mode "fsdax"
test_mode "sector"
_cleanup
exit 0
