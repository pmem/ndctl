#!/bin/bash
MNT=test_dax_mnt
FILE=image
NDCTL="./ndctl"
json2var="s/[{}\",]//g; s/:/=/g"
blockdev=""

err() {
	rc=1
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

blockdev=$(basename $(test/dax-dev))
dev=$(basename $(readlink -f /sys/block/$(basename $blockdev)/device))

mkfs.ext4 /dev/$blockdev
mount /dev/$blockdev $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
test/dax-pmd $MNT/$FILE
umount $MNT

# convert pmem to put the memmap on the device
json=$($NDCTL create-namespace -m memory -M dev -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "memory" ] && echo "fail: $LINENO" &&  exit 1

#note the blockdev returned from ndctl create-namespace lacks the /dev prefix
mkfs.ext4 /dev/$blockdev
mount /dev/$blockdev $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
test/dax-pmd $MNT/$FILE
umount $MNT

json=$($NDCTL create-namespace -m raw -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "memory" ] && echo "fail: $LINENO" &&  exit 1

mkfs.xfs -f /dev/$blockdev
mount /dev/$blockdev $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
test/dax-pmd $MNT/$FILE
umount $MNT

# convert pmem to put the memmap on the device
json=$($NDCTL create-namespace -m memory -M dev -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "memory" ] && echo "fail: $LINENO" &&  exit 1

mkfs.xfs -f /dev/$blockdev
mount /dev/$blockdev $MNT -o dax
fallocate -l 1GiB $MNT/$FILE
test/dax-pmd $MNT/$FILE
umount $MNT

# revert namespace to raw mode
json=$($NDCTL create-namespace -m raw -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "memory" ] && echo "fail: $LINENO" &&  exit 1

exit 0
