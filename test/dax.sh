#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

. $(dirname $0)/common

MNT=test_dax_mnt
FILE=image
blockdev=""

cleanup() {
	echo "test-dax: failed at line $1"
	if [ -n "$blockdev" ]; then
		umount /dev/$blockdev
	else
		rc=77
	fi
	rmdir $MNT
	exit $rc
}

run_test() {
	rc=0
	if ! trace-cmd record -e fs_dax:dax_pmd_fault_done ./dax-pmd $MNT/$FILE; then
		rc=$?
		if [ "$rc" -ne 77 ] && [ "$rc" -ne 0 ]; then
			cleanup "$1"
		fi
	fi

	# Fragile hack to double check the kernel services this test
	# with successful pmd faults. If dax-pmd.c ever changes the
	# number of times the dax_pmd_fault_done trace point fires the
	# hack needs to be updated from 10 expected firings and the
	# result of success (NOPAGE).
	count=0
	rc=1
	while read -r p; do
		[[ $p ]] || continue
		if [ "$count" -lt 10 ]; then
			if [ "$p" != "0x100" ] && [ "$p" != "NOPAGE" ]; then
				cleanup "$1"
			fi
		fi
		count=$((count + 1))
	done < <(trace-cmd report | awk '{ print $21 }')

	if [ $count -lt 10 ]; then
		cleanup "$1"
	fi
}

run_ext4() {
	mkfs.ext4 -b 4096 /dev/$blockdev
	mount /dev/$blockdev $MNT -o dax
	fallocate -l 1GiB $MNT/$FILE
	run_test $LINENO
	umount $MNT

	# convert pmem to put the memmap on the device
	json=$($NDCTL create-namespace -m fsdax -M dev -f -e $dev)
	eval $(json2var <<< "$json")
	[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1
	#note the blockdev returned from ndctl create-namespace lacks the /dev prefix

	mkfs.ext4 -b 4096 /dev/$blockdev
	mount /dev/$blockdev $MNT -o dax
	fallocate -l 1GiB $MNT/$FILE
	run_test $LINENO
	umount $MNT
	json=$($NDCTL create-namespace -m raw -f -e $dev)

	eval $(json2var <<< "$json")
	[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1
	true
}

run_xfs() {
	mkfs.xfs -f -d su=2m,sw=1,agcount=2 -m reflink=0 /dev/$blockdev
	mount /dev/$blockdev $MNT -o dax
	fallocate -l 1GiB $MNT/$FILE
	run_test $LINENO
	umount $MNT

	# convert pmem to put the memmap on the device
	json=$($NDCTL create-namespace -m fsdax -M dev -f -e $dev)
	eval $(json2var <<< "$json")
	[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1
	mkfs.xfs -f -d su=2m,sw=1,agcount=2 -m reflink=0 /dev/$blockdev

	mount /dev/$blockdev $MNT -o dax
	fallocate -l 1GiB $MNT/$FILE
	run_test $LINENO
	umount $MNT
	# revert namespace to raw mode

	json=$($NDCTL create-namespace -m raw -f -e $dev)
	eval $(json2var <<< "$json")
	[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1
	true
}

set -e
mkdir -p $MNT
trap 'err $LINENO cleanup' ERR

dev=$(./dax-dev)
json=$($NDCTL list -N -n $dev)
eval $(json2var <<< "$json")
rc=1

if [ $(basename $0) = "dax-ext4.sh" ]; then
	run_ext4
elif [ $(basename $0) = "dax-xfs.sh" ]; then
	run_xfs
else
	run_ext4
	run_xfs
fi

exit 0
