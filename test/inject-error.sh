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
err_block=42
err_count=8

trap 'err $LINENO' ERR

# sample json:
#{
#  "dev":"namespace7.0",
#  "mode":"memory",
#  "size":"60.00 MiB (62.92 MB)",
#  "uuid":"f1baa71a-d165-4da4-bb6a-083a2b0e6469",
#  "blockdev":"pmem7",
#}

# $1: Line number
# $2: exit code
err()
{
	[ -n "$2" ] && rc="$2"
	echo "test/inject-error.sh: failed at line $1"
	exit "$rc"
}

check_min_kver()
{
	local ver="$1"
	: "${KVER:=$(uname -r)}"

	[ -n "$ver" ] || return 1
	[[ "$ver" == "$(echo -e "$ver\n$KVER" | sort -V | head -1)" ]]
}

check_min_kver "4.15" || { echo "kernel $KVER may not support error injection"; exit "$rc"; }

create()
{
	json=$($ndctl create-namespace -b "$bus" -t pmem --align=4k)
	eval "$(echo "$json" | sed -e "$json2var")"
	[ -n "$dev" ] || err "$LINENO" 2
	[ -n "$size" ] || err "$LINENO" 2
	[ -n "$blockdev" ] || err "$LINENO" 2
	[ $size -gt 0 ] || err "$LINENO" 2
}

reset()
{
	$ndctl disable-region -b "$bus" all
	$ndctl zero-labels -b "$bus" all
	$ndctl enable-region -b "$bus" all
}

check_status()
{
	local sector="$1"
	local count="$2"

	json="$($ndctl inject-error --status $dev)"
	[[ "$sector" == "$(jq ".badblocks[0].block" <<< "$json")" ]]
	[[ "$count" == "$(jq ".badblocks[0].count" <<< "$json")" ]]
}

do_tests()
{
	# inject without notification
	$ndctl inject-error --block=$err_block --count=$err_count --no-notify $dev
	check_status "$err_block" "$err_count"
	if read -r sector len < /sys/block/$blockdev/badblocks; then
		# fail if reading badblocks returns data
		echo "fail: $LINENO" && exit 1
	fi

	# clear via err-inj-clear
	$ndctl inject-error --block=$err_block --count=$err_count --uninject $dev
	check_status

	# inject normally
	$ndctl inject-error --block=$err_block --count=$err_count $dev
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
exit 0
