#!/bin/bash

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

MNT=test_mmap_mnt
FILE=image
DEV=""
TEST=./mmap
NDCTL="../ndctl/ndctl"
json2var="s/[{}\",]//g; s/:/=/g"

err() {
	rc=1
	echo "test-mmap: failed at line $1"
	if [ -n "$DEV" ]; then
		umount $DEV
	else
		rc=77
	fi
	rmdir $MNT
	exit $rc
}

test_mmap() {
	trap 'err $LINENO' ERR

	# SHARED
	$TEST -Mrwps $MNT/$FILE     # mlock, populate, shared (mlock fail)
	$TEST -Arwps $MNT/$FILE     # mlockall, populate, shared
	$TEST -RMrps $MNT/$FILE     # read-only, mlock, populate, shared (mlock fail)
	$TEST -rwps  $MNT/$FILE     # populate, shared (populate no effect)
	$TEST -Rrps  $MNT/$FILE     # read-only populate, shared (populate no effect)
	$TEST -Mrws  $MNT/$FILE     # mlock, shared (mlock fail)
	$TEST -RMrs  $MNT/$FILE     # read-only, mlock, shared (mlock fail)
	$TEST -rws   $MNT/$FILE     # shared (ok)
	$TEST -Rrs   $MNT/$FILE     # read-only, shared (ok)

	# PRIVATE
	$TEST -Mrwp  $MNT/$FILE      # mlock, populate, private (ok)
	$TEST -RMrp  $MNT/$FILE      # read-only, mlock, populate, private (mlock fail)
	$TEST -rwp   $MNT/$FILE      # populate, private (ok)
	$TEST -Rrp   $MNT/$FILE      # read-only, populate, private (populate no effect)
	$TEST -Mrw   $MNT/$FILE      # mlock, private (ok)
	$TEST -RMr   $MNT/$FILE      # read-only, mlock, private (mlock fail)
	$TEST -MSr   $MNT/$FILE      # private, read before mlock (ok)
	$TEST -rw    $MNT/$FILE      # private (ok)
	$TEST -Rr    $MNT/$FILE      # read-only, private (ok)
}

set -e
mkdir -p $MNT
trap 'err $LINENO' ERR

dev=$(./dax-dev)
json=$($NDCTL list -N -n $dev)
eval $(echo $json | sed -e "$json2var")
DEV="/dev/${blockdev}"

mkfs.ext4 $DEV
mount $DEV $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
test_mmap
umount $MNT

mkfs.xfs -f $DEV
mount $DEV $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
test_mmap
umount $MNT
