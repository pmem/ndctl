#!/bin/bash -x

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

DEV=""
NDCTL="../ndctl/ndctl"
BUS="-b nfit_test.0"
BUS1="-b nfit_test.1"
json2var="s/[{}\",]//g; s/:/=/g"
rc=77

set -e

err() {
	echo "test/clear: failed at line $1"
	exit $rc
}

check_min_kver()
{
	local ver="$1"
	: "${KVER:=$(uname -r)}"

	[ -n "$ver" ] || return 1
	[[ "$ver" == "$(echo -e "$ver\n$KVER" | sort -V | head -1)" ]]
}

check_min_kver "4.6" || { echo "kernel $KVER lacks clear poison support"; exit $rc; }

set -e
trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region $BUS all
$NDCTL zero-labels $BUS all
$NDCTL enable-region $BUS all

rc=1

# create pmem
dev="x"
json=$($NDCTL create-namespace $BUS -t pmem -m raw)
eval $(echo $json | sed -e "$json2var")
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1
[ $mode != "raw" ] && echo "fail: $LINENO" && exit 1

# inject errors in the middle of the namespace, verify that reading fails
err_sector="$(((size/512) / 2))"
err_count=8
$NDCTL inject-error --block="$err_sector" --count=$err_count $dev
read sector len < /sys/block/$blockdev/badblocks
[ $((sector * 2)) -ne $((size /512)) ] && echo "fail: $LINENO" && exit 1
if dd if=/dev/$blockdev of=/dev/null iflag=direct bs=512 skip=$sector count=$len; then
	echo "fail: $LINENO" && exit 1
fi

size_raw=$size
sector_raw=$sector

# convert pmem to memory mode
json=$($NDCTL create-namespace -m memory -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "memory" ] && echo "fail: $LINENO" && exit 1

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

if check_min_kver "4.9.0"; then
	# check for re-appearance of stale badblocks from poison_list
	$NDCTL disable-region $BUS all
	$NDCTL enable-region $BUS all

	# since we have cleared the errors, a disable/reenable shouldn't bring them back
	if read sector len < /sys/block/$blockdev/badblocks; then
		# fail if reading badblocks returns data
		echo "fail: $LINENO" && exit 1
	fi
fi

$NDCTL disable-region $BUS all
$NDCTL disable-region $BUS1 all
modprobe -r nfit_test

exit 0
