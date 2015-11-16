#!/bin/bash
MNT=test_dax_mnt
FILE=image
DEV=""

err() {
	rc=1
	echo "test-dax: failed at line $1"
	if [ -n "$DEV" ]; then
		umount $DEV
	else
		rc=77
	fi
	rmdir $MNT
	exit $rc
}

set -e
mkdir -p $MNT
trap 'err $LINENO' ERR

DEV=$(lib/test-dax-dev)

mkfs.ext4 $DEV
mount $DEV $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
lib/test-dax-pmd $MNT/$FILE
umount $MNT

mkfs.xfs -f $DEV
mount $DEV $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
lib/test-dax-pmd $MNT/$FILE
umount $MNT
