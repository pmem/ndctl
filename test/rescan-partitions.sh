#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright(c) 2018 Intel Corporation. All rights reserved.

bus="nfit_test.0"
json2var="s/[{}\",]//g; s/:/=/g"
dev=""
size=""
blockdev=""
rc=77

. ./common

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
	$ndctl disable-region -b "$bus" all
	$ndctl zero-labels -b "$bus" all
	$ndctl enable-region -b "$bus" all
}

test_mode()
{
	local mode="$1"

	# create namespace
	json=$($ndctl create-namespace -b "$bus" -t pmem -m "$mode")
	rc=2
	eval "$(echo "$json" | sed -e "$json2var")"
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
	$ndctl disable-namespace $dev
	$ndctl enable-namespace $dev
	if [ -b /dev/$partdev ]; then
		echo "mode: $mode - partition read successful"
	else
		echo "mode: $mode - partition read failed"
		rc=1
		err "$LINENO"
	fi

	$ndctl disable-namespace $dev
	$ndctl destroy-namespace $dev
}

modprobe nfit_test
rc=1
reset
test_mode "raw"
test_mode "fsdax"
test_mode "sector"

exit 0
