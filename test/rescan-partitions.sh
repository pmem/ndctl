#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright(c) 2018 Intel Corporation. All rights reserved.

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
#  "dev":"namespace5.0",
#  "mode":"sector",
#  "size":"60.00 MiB (62.92 MB)",
#  "uuid":"f1baa71a-d165-4da4-bb6a-083a2b0e6469",
#  "blockdev":"pmem5s",
#}

# $1: Line number
# $2: exit code
err()
{
	[ -n "$2" ] && rc="$2"
	echo "test/rescan-partitions.sh: failed at line $1"
	exit "$rc"
}

check_min_kver()
{
	local ver="$1"
	: "${KVER:=$(uname -r)}"

	[ -n "$ver" ] || return 1
	[[ "$ver" == "$(echo -e "$ver\n$KVER" | sort -V | head -1)" ]]
}
check_min_kver "4.16" || { echo "kernel $KVER may not contain fixes for partition rescanning"; exit "$rc"; }

check_prereq()
{
	if ! command -v "$1" >/dev/null; then
		echo "missing '$1', skipping.."
		exit "$rc"
	fi
}
check_prereq "parted"
check_prereq "blockdev"

reset()
{
	modprobe nfit_test
	$ndctl disable-region -b "$bus" all
	$ndctl zero-labels -b "$bus" all
	$ndctl enable-region -b "$bus" all
}

test_mode()
{
	local mode="$1"

	# create namespace
	json=$($ndctl create-namespace -b "$bus" -t pmem -m "$mode")
	eval "$(echo "$json" | sed -e "$json2var")"
	[ -n "$dev" ] || err "$LINENO" 2
	[ -n "$size" ] || err "$LINENO" 2
	[ -n "$blockdev" ] || err "$LINENO" 2
	[ $size -gt 0 ] || err "$LINENO" 2

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
		err "$LINENO" 1
	fi

	$ndctl disable-namespace $dev
	$ndctl destroy-namespace $dev
}

rc=1
reset
test_mode "raw"
test_mode "memory"
test_mode "sector"

exit 0
