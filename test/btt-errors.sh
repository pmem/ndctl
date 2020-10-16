#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

MNT=test_btt_mnt
FILE=image
blockdev=""
rc=77

. $(dirname $0)/common

cleanup()
{
	rm -f $FILE
	rm -f $MNT/$FILE
	if grep -q "$MNT" /proc/mounts; then
		umount $MNT
	else
		rc=77
	fi
	rmdir $MNT
}

force_raw()
{
	raw="$1"
	if grep -q "$MNT" /proc/mounts; then umount $MNT; fi
	$NDCTL disable-namespace "$dev"
	echo "$raw" > "/sys/bus/nd/devices/$dev/force_raw"
	$NDCTL enable-namespace "$dev"
	echo "Set $dev to raw mode: $raw"
	if [[ "$raw" == "1" ]]; then
		raw_bdev=${blockdev%s}
		test -b "/dev/$raw_bdev"
	else
		raw_bdev=""
	fi
}

check_min_kver "4.15" || do_skip "may lack BTT error handling"

set -e
mkdir -p $MNT
trap 'err $LINENO cleanup' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all

rc=1

# create a btt namespace and clear errors (if any)
dev="x"
json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem -m sector)
eval "$(echo "$json" | json2var)"
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1

force_raw 1
if read -r sector len < "/sys/block/$raw_bdev/badblocks"; then
	dd of=/dev/$raw_bdev if=/dev/zero oflag=direct bs=512 seek="$sector" count="$len"
fi
force_raw 0

mkfs.ext4 "/dev/$blockdev" -b 4096
mount -o nodelalloc "/dev/$blockdev" $MNT

# prepare an image file with random data
dd if=/dev/urandom of=$FILE bs=4096 count=1
test -s $FILE

# copy it to the file system
cp $FILE $MNT/$FILE

# Get the start sector for the file
start_sect=$(filefrag -v -b512 $MNT/$FILE | grep -E "^[ ]+[0-9]+.*" | head -1 | awk '{ print $4 }' | cut -d. -f1)
start_4k=$((start_sect/8))
test -n "$start_sect"
echo "start sector of the file is: $start_sect (512B) or $start_4k (4096B)"

# figure out the btt offset

force_raw 1

# calculate start of the map
map=$(hexdump -s 96 -n 4 "/dev/$raw_bdev" | head -1 | cut -d' ' -f2-)
map=$(tr -d ' ' <<< "0x${map#* }${map%% *}")
printf "btt map starts at: %x\n" "$map"

# calculate map entry byte offset for the file's block
map_idx=$((map + (4 * start_4k)))
printf "btt map entry location for sector %x: %x\n" "$start_4k" "$map_idx"

# read the map entry
map_ent=$(hexdump -s $map_idx -n 4 "/dev/$raw_bdev" | head -1 | cut -d' ' -f2-)
map_ent=$(tr -d ' ' <<< "0x${map_ent#* }${map_ent%% *}")
map_ent=$((map_ent & 0x3fffffff))
printf "btt map entry: 0x%x\n" "$map_ent"

# calculate the data offset
dataoff=$(((map_ent * 4096) + 4096))
printf "dataoff: 0x%x\n" "$dataoff"

bb_inj=$((dataoff/512))

# inject badblocks for one page at the start of the file
$NDCTL inject-error --block="$bb_inj" --count=8 $dev
$NDCTL start-scrub $NFIT_TEST_BUS0 && $NDCTL wait-scrub $NFIT_TEST_BUS0

force_raw 0
mount -o nodelalloc "/dev/$blockdev" $MNT

# make sure reading the first block of the file fails as expected
: The following 'dd' is expected to hit an I/O Error
dd if=$MNT/$FILE of=/dev/null iflag=direct bs=4096 count=1 && err $LINENO || true

# write via btt to clear the error
dd if=/dev/zero of=$MNT/$FILE oflag=direct bs=4096 count=1

# read again and that should succeed
dd if=$MNT/$FILE of=/dev/null iflag=direct bs=4096 count=1


## ensure we get an EIO for errors in namespace metadata

# reset everything to get a clean log
if grep -q "$MNT" /proc/mounts; then umount $MNT; fi
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all
dev="x"
json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem -m sector)
eval "$(echo "$json" | json2var)"
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1

# insert error at an arbitrary offset in the map (sector 0)
force_raw 1
map=$(hexdump -s 96 -n 4 "/dev/$raw_bdev" | head -1 | cut -d' ' -f2-)
map=$(tr -d ' ' <<< "0x${map#* }${map%% *}")
bb_inj=$((map/512))
$NDCTL inject-error --block="$bb_inj" --count=1 $dev
$NDCTL start-scrub $NFIT_TEST_BUS0 && $NDCTL wait-scrub $NFIT_TEST_BUS0
force_raw 0

# make sure reading the first block of the namespace fails
: The following 'dd' is expected to hit an I/O Error
dd if=/dev/$blockdev of=/dev/null iflag=direct bs=4096 count=1 && err $LINENO || true

# done, exit
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all
cleanup
_cleanup
exit 0
