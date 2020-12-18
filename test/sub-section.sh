#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

set -e

SKIP=77
FAIL=1
SUCCESS=0

. ./common

check_min_kver "5.3" || do_skip "may lack align sub-section hotplug support"

MNT=test_dax_mnt
mkdir -p $MNT

TEST_SIZE=$((16<<20))
MIN_AVAIL=$((TEST_SIZE*4))
MAX_NS=10
NAME="subsection-test"

ndctl list -N | jq -r ".[] | select(.name==\"subsection-test\") | .dev"

rc=$FAIL
cleanup() {
	if [ $rc -ne $SUCCESS ]; then
		echo "test/sub-section.sh: failed at line $1"
	fi
	if mountpoint -q $MNT; then
		umount $MNT
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

json=$($NDCTL list -R -b ACPI.NFIT)
region=$(echo $json | jq -r "[.[] | select(.available_size >= $MIN_AVAIL)][0].dev")
avail=$(echo $json | jq -r "[.[] | select(.available_size >= $MIN_AVAIL)][0].available_size")
if [ -z $region ]; then
	exit $SKIP
fi

iter=$((avail/TEST_SIZE))
if [ $iter -gt $MAX_NS ]; then
	iter=$MAX_NS;
fi

for i in $(seq 1 $iter)
do
	json=$($NDCTL create-namespace -s $TEST_SIZE --no-autorecover -r $region -n "$NAME")
	dev=$(echo $json | jq -r ".blockdev")
	mkfs.ext4 -b 4096 /dev/$dev
	mount -o dax /dev/$dev $MNT
	umount $MNT
done

namespaces=$($NDCTL list -N | jq -r ".[] | select(.name==\"$NAME\") | .dev")
for i in $namespaces
do
	$NDCTL disable-namespace $i
	$NDCTL enable-namespace $i
	$NDCTL destroy-namespace $i -f
done

rc=$SUCCESS
cleanup $LINENO
