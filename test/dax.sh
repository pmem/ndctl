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

MNT=test_dax_mnt
FILE=image
NDCTL="../ndctl/ndctl"
json2var="s/[{}\",]//g; s/:/=/g"
blockdev=""

err() {
	echo "test-dax: failed at line $1"
	if [ -n "$blockdev" ]; then
		umount /dev/$blockdev
	else
		rc=77
	fi
	rmdir $MNT
	exit $rc
}

set -e
mkdir -p $MNT
trap 'err $LINENO' ERR

dev=$(./dax-dev)
json=$($NDCTL list -N -n $dev)
eval $(echo $json | sed -e "$json2var")
rc=1

mkfs.ext4 /dev/$blockdev
mount /dev/$blockdev $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
./dax-pmd $MNT/$FILE
umount $MNT

# convert pmem to put the memmap on the device
json=$($NDCTL create-namespace -m fsdax -M dev -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1

#note the blockdev returned from ndctl create-namespace lacks the /dev prefix
mkfs.ext4 /dev/$blockdev
mount /dev/$blockdev $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
./dax-pmd $MNT/$FILE
umount $MNT

json=$($NDCTL create-namespace -m raw -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1

mkfs.xfs -f /dev/$blockdev
mount /dev/$blockdev $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
./dax-pmd $MNT/$FILE
umount $MNT

# convert pmem to put the memmap on the device
json=$($NDCTL create-namespace -m fsdax -M dev -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1

mkfs.xfs -f /dev/$blockdev
mount /dev/$blockdev $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
./dax-pmd $MNT/$FILE
umount $MNT

# revert namespace to raw mode
json=$($NDCTL create-namespace -m raw -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "fsdax" ] && echo "fail: $LINENO" &&  exit 1

exit 0
