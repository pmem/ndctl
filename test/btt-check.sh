#!/bin/bash -E

[ -f "../ndctl/ndctl" ] && [ -x "../ndctl/ndctl" ] && ndctl="../ndctl/ndctl"
[ -f "./ndctl/ndctl" ] && [ -x "./ndctl/ndctl" ] && ndctl="./ndctl/ndctl"
[ -z "$ndctl" ] && echo "Couldn't find an ndctl binary" && exit 1
bus="nfit_test.0"
json2var="s/[{}\",]//g; s/:/=/g"
dev=""
mode=""
size=""
sector_size=""
blockdev=""
bs=4096
rc=77

trap 'err $LINENO' ERR

# sample json:
# {
#   "dev":"namespace5.0",
#   "mode":"sector",
#   "size":32440320,
#   "uuid":"51805176-e124-4635-ae17-0e6a4a16671a",
#   "sector_size":4096,
#   "blockdev":"pmem5s"
# }

# $1: Line number
# $2: exit code
err()
{
	[ -n "$2" ] && rc="$2"
	echo "test/btt-check: failed at line $1"
	exit "$rc"
}

check_min_kver()
{
	local ver="$1"
	: "${KVER:=$(uname -r)}"

	[ -n "$ver" ] || return 1
	[[ "$ver" == "$(echo -e "$ver\n$KVER" | sort -V | head -1)" ]]
}

check_min_kver "4.12" || { echo "kernel $KVER may not support badblocks clearing on pmem via btt"; exit $rc; }

create()
{
	json=$($ndctl create-namespace -b "$bus" -t pmem -m sector)
	eval "$(echo "$json" | sed -e "$json2var")"
	[ -n "$dev" ] || err "$LINENO" 2
	[ "$mode" = "sector" ] || err "$LINENO" 2
	[ -n "$size" ] || err "$LINENO" 2
	[ -n "$sector_size" ] || err "$LINENO" 2
	[ -n "$blockdev" ] || err "$LINENO" 2
	[ $size -gt 0 ] || err "$LINENO" 2
}

reset()
{
	$ndctl disable-region -b "$bus" all
	$ndctl zero-labels -b "$bus" all
	$ndctl enable-region -b "$bus" all
}

# re-enable the BTT namespace, and do IO to it in an attempt to
# verify it still comes up ok, and functions as expected
post_repair_test()
{
	echo "${FUNCNAME[0]}: I/O to BTT namespace"
	test -b /dev/$blockdev
	dd if=/dev/urandom of=test-bin bs=$sector_size count=$((size/sector_size)) > /dev/null 2>&1
	dd if=test-bin of=/dev/$blockdev bs=$sector_size count=$((size/sector_size)) > /dev/null 2>&1
	dd if=/dev/$blockdev of=test-bin-read bs=$sector_size count=$((size/sector_size)) > /dev/null 2>&1
	diff test-bin test-bin-read
	rm -f test-bin*
	echo "done"
}

test_normal()
{
	echo "=== ${FUNCNAME[0]} ==="
	# disable the namespace
	$ndctl disable-namespace $dev
	$ndctl check-namespace $dev
	$ndctl enable-namespace $dev
	post_repair_test
}

test_force()
{
	echo "=== ${FUNCNAME[0]} ==="
	$ndctl check-namespace --force $dev
	post_repair_test
}

set_raw()
{
	$ndctl disable-namespace $dev
	echo -n "set raw_mode: "
	echo 1 | tee /sys/bus/nd/devices/$dev/force_raw
	$ndctl enable-namespace $dev
	raw_bdev="${blockdev%%s}"
	test -b /dev/$raw_bdev
	raw_size="$(cat /sys/bus/nd/devices/$dev/size)"
}

unset_raw()
{
	$ndctl disable-namespace $dev
	echo -n "set raw_mode: "
	echo 0 | tee /sys/bus/nd/devices/$dev/force_raw
	$ndctl enable-namespace $dev
	raw_bdev=""
}

test_bad_info2()
{
	echo "=== ${FUNCNAME[0]} ==="
	set_raw
	seek="$((raw_size/bs - 1))"
	echo "wiping info2 block (offset = $seek blocks)"
	dd if=/dev/zero of=/dev/$raw_bdev bs=$bs count=1 seek=$seek
	unset_raw
	$ndctl disable-namespace $dev
	$ndctl check-namespace $dev 2>&1 | grep "info2 needs to be restored"
	$ndctl check-namespace --repair $dev
	$ndctl enable-namespace $dev
	post_repair_test
}

test_bad_info()
{
	echo "=== ${FUNCNAME[0]} ==="
	set_raw
	echo "wiping info block"
	dd if=/dev/zero of=/dev/$raw_bdev bs=$bs count=1 seek=1
	unset_raw
	$ndctl disable-namespace $dev
	$ndctl check-namespace $dev 2>&1 | grep "info block at offset 0x1000 needs to be restored"
	$ndctl check-namespace --repair $dev
	$ndctl enable-namespace $dev
	post_repair_test
}

test_bitmap()
{
	echo "=== ${FUNCNAME[0]} ==="
	reset && create
	set_raw
	# scribble over the last 4K of the map
	rm -f /tmp/scribble
	for (( i=0 ; i<512 ; i++ )); do
		echo -n -e \\x1e\\x1e\\x00\\xc0\\x1e\\x1e\\x00\\xc0 >> /tmp/scribble
	done
	seek="$((raw_size/bs - (256*64/bs) - 2))"
	echo "scribbling over map entries (offset = $seek blocks)"
	dd if=/tmp/scribble of=/dev/$raw_bdev bs=$bs seek=$seek
	rm -f /tmp/scribble
	unset_raw
	$ndctl disable-namespace $dev
	$ndctl check-namespace $dev 2>&1 | grep "bitmap error"
	# This is not repairable
	reset && create
}

do_tests()
{
	test_normal
	test_force
	test_bad_info2
	test_bad_info
	test_bitmap
}

# setup (reset nfit_test dimms, create the BTT namespace)
modprobe nfit_test
rc=1
reset && create
do_tests
reset
exit 0
