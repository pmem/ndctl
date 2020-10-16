#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

set -e

SKIP=77
FAIL=1
SUCCESS=0

. ./common

MNT=test_dax_mnt
TEST_DM_PMEM=/dev/mapper/test_pmem
NAME=$(basename $TEST_DM_PMEM)

mkdir -p $MNT

TEST_SIZE=$((1<<30))

rc=$FAIL
cleanup() {
	if [ $rc -ne $SUCCESS ]; then
		echo "test/dm.sh: failed at line $1"
	fi
	if mountpoint -q $MNT; then
		umount $MNT
	fi

	if [ -L $TEST_DM_PMEM ]; then
		dmsetup remove $TEST_DM_PMEM
	fi
	rmdir $MNT
	# opportunistic cleanup, not fatal if these fail
	namespaces=$($NDCTL list -N | jq -r ".[] | select(.name==\"$NAME\") | .dev")
	for i in $namespaces
	do
		if ! $NDCTL destroy-namespace -f $i; then
			echo "test/sub-section.sh: cleanup() failed to destroy $i"
		fi
	done
	exit $rc
}

trap 'err $LINENO cleanup' ERR

dev="x"
json=$($NDCTL create-namespace -b ACPI.NFIT -s $TEST_SIZE -t pmem -m fsdax -n "$NAME")
eval $(echo $json | json2var )
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1
[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1

pmem0=/dev/$blockdev
size0=$((size/512))

json=$($NDCTL create-namespace -b ACPI.NFIT -s $TEST_SIZE -t pmem -m fsdax -n "$NAME")
eval $(echo $json | json2var )
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1
[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1

pmem1=/dev/$blockdev
size1=$((size/512))

cat <<EOF |
0 $size0 linear $pmem0 0
$size0 $size1 linear $pmem1 0
EOF
dmsetup create $(basename $NAME)

mkfs.ext4 -b 4096 $TEST_DM_PMEM
mount -o dax $TEST_DM_PMEM $MNT
umount $MNT

rc=$SUCCESS
cleanup $LINENO
